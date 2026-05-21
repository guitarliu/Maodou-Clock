import asyncio
import json
import os
import glob
import base64
import re
import logging
import websockets

# 开启调试日志（可选，便于观察连接细节）
logging.basicConfig(level=logging.INFO)

# ---------- 配置 ----------
# 【Hermes-Agent 默认会话路径】
SESSIONS_DIR = os.path.expanduser("~/.hermes/sessions")

# -------------------------------------------------------------
# 【小龙虾 OpenClaw 适配提示 - 第一步】
# 如果你要适配小龙虾，请注释掉上面的 SESSIONS_DIR，并启用下方适合你小龙虾配置的路径：
# 1. 默认全局会话路径：
# SESSIONS_DIR = os.path.expanduser("~/.openclaw/sessions")
# 2. 指定智能体（Agent）的会话路径：
# SESSIONS_DIR = os.path.expanduser("~/.openclaw/agents/【填入你的智能体ID】/sessions")
# -------------------------------------------------------------

WS_HOST = "0.0.0.0"
WS_PORT = 9119
POLL_INTERVAL = 1.0

connected_clients = {}   # key: "ip:port"
last_size = 0
current_file = None

def find_latest_session():
    # 【Hermes-Agent 默认文件匹配规则】：session_*.json
    pattern = os.path.join(SESSIONS_DIR, "session_*.json")
    
    # -------------------------------------------------------------
    # 【小龙虾 OpenClaw 适配提示 - 第二步】
    # 小龙虾的 JSON 会话文件可能命名不同（例如 uuid.json），如果是这样，请启用下方通配所有 json 文件的规则：
    # pattern = os.path.join(SESSIONS_DIR, "*.json")
    # -------------------------------------------------------------
    
    files = glob.glob(pattern)
    if not files:
        return None
    return max(files, key=os.path.getmtime)

def extract_messages_and_meta(data):
    """返回 (messages, is_active)"""
    is_active = True
    
    # -------------------------------------------------------------
    # 【小龙虾 OpenClaw 适配提示 - 第三步】
    # 如果小龙虾保存的会话 JSON 最外层键名与 Hermes 不同，请在这里适配：
    # 示例一：如果小龙虾把聊天历史存放在 "history" 或 "chat_history" 键中，可在这里解析：
    # if isinstance(data, dict):
    #     messages = data.get("history", [])  # 或者 data.get("chat_history", [])
    #     is_active = data.get("is_active", True)
    # -------------------------------------------------------------
    
    if isinstance(data, list):
        messages = data
    elif isinstance(data, dict):
        messages = data.get("messages", [])
        is_active = data.get("is_active", True)
    else:
        messages = []
    return messages, is_active

def parse_messages(messages, is_active=True):
    if not messages:
        return {
            "status": "等待任务",
            "task": "会话刚刚创建",
            "progress": 5,
            "tokens": 0,
            "result": ""
        }

    # -------------------------------------------------------------
    # 【小龙虾 OpenClaw 适配提示 - 第四步】
    # 检查小龙虾 JSON 中的角色定义字段（Role）。
    # 如果它的 Role 不是 "user"、"assistant"、"tool"（例如使用了 "human" 或 "ai" 等），请修改下方的筛选条件：
    # user_msgs = [m for m in messages if m.get('role') in ('user', 'human')]
    # assistant_msgs = [m for m in messages if m.get('role') in ('assistant', 'ai')]
    # -------------------------------------------------------------

    user_msgs = [m for m in messages if m.get('role') == 'user']
    if user_msgs:
        task = user_msgs[-1].get('content', '')
        # 【过滤】：在源头过滤掉任务文本中的 Markdown 加粗符号 **
        task = task.replace('**', '')
        if len(task) > 48:
            task = task[:48] + "..."
    else:
        task = "等待输入..."

    assistant_msgs = [m for m in messages if m.get('role') == 'assistant']
    tool_msgs = [m for m in messages if m.get('role') == 'tool']

    # ---------- 判断是否已完成 ----------
    finished = False
    # 顶层标记会话结束
    if not is_active:
        finished = True

    last_assistant = None
    if assistant_msgs:
        last_assistant = assistant_msgs[-1]
        finish_reason = last_assistant.get("finish_reason", "").lower()
        if finish_reason in ("stop", "end_turn", "tool_calls"):
            if finish_reason != "tool_calls":
                if not tool_msgs or messages[-1].get('role') == 'assistant':
                    finished = True

    # 最后一条是 assistant 且之后无工具调用
    if not finished and assistant_msgs and messages[-1].get('role') == 'assistant':
        last_idx = messages.index(messages[-1])
        subsequent_tools = any(m.get('role') == 'tool' for m in messages[last_idx+1:])
        if not subsequent_tools and len(last_assistant.get('content', '')) > 0:
            finished = True

    # ---------- 状态与进度 ----------
    if finished:
        status, progress = "已完成", 100
    elif not assistant_msgs and not tool_msgs:
        status, progress = "等待响应", 15
    elif tool_msgs and messages[-1].get('role') == 'tool':
        status, progress = "工具调用中", 60
    elif assistant_msgs and len(last_assistant.get('content','')) > 50:
        status, progress = "生成回复中", 85
    else:
        status, progress = "运行中", 40

    # ---------- Token 估算 ----------
    total_chars = sum(len(m.get('content','')) for m in messages)
    tokens = total_chars // 3

    # ---------- 提取最终回复内容（清理特殊符号） ----------
    result_text = ""
    if assistant_msgs:
        result_text = last_assistant.get('content', '')
        # 【过滤】：在源头过滤掉回复文本中的 Markdown 加粗符号 **
        result_text = result_text.replace('**', '')
        
        # 将换行、回车、制表符替换为空格，并压缩多余空格
        result_text = result_text.replace('\n', ' ').replace('\r', ' ').replace('\t', ' ')
        result_text = re.sub(r'\s+', ' ', result_text).strip()
        # 限制长度，避免滚动太多
        if len(result_text) > 500:
            result_text = result_text[:500] + "..."

    return {
        "status": status,
        "task": task,
        "progress": progress,
        "tokens": tokens,
        "result": result_text
    }

def read_new_data():
    global last_size, current_file
    try:
        latest = find_latest_session()
        if not latest:
            return {"status": "等待任务", "task": "暂无会话", "progress": 0, "tokens": 0, "result": ""}
        if latest != current_file:
            current_file = latest
            last_size = 0
            print(f"自动跟踪最新会话: {os.path.basename(latest)}")

        current_size = os.path.getsize(current_file)
        if current_size == last_size:
            return None
        last_size = current_size

        with open(current_file, 'r', encoding='utf-8') as f:
            data = json.load(f)

        messages, is_active = extract_messages_and_meta(data)
        if messages:
            print(f"成功提取 {len(messages)} 条消息")
        else:
            print("警告：未找到消息列表，检查JSON结构")
        return parse_messages(messages, is_active)

        # -------------------------------------------------------------
        # 【小龙虾 OpenClaw 适配提示 - 第五步（如果是 JSONL 追加写格式）】
        # 如果你的小龙虾将会话存成了追加式的 jsonl 格式，而不是整个 json 对象，则需要用按行读取的方式提取：
        # with open(current_file, 'r', encoding='utf-8') as f:
        #     lines = f.readlines()
        # messages = [json.loads(line) for line in lines if line.strip()]
        # is_active = True # 或者根据最后一行状态判断
        # return parse_messages(messages, is_active)
        # -------------------------------------------------------------

    except json.JSONDecodeError:
        return None
    except Exception as e:
        print(f"读取异常: {e}")
        return None

async def broadcast(message):
    if connected_clients:
        payload = json.dumps(message)
        await asyncio.gather(*[ws.send(payload) for ws in connected_clients.values()])

async def poll_loop():
    print("开始自动监控最新会话文件...")
    last_payload = None
    while True:
        result = read_new_data()
        if result:
            last_payload = result
            print(f"推送: {result}")
        if connected_clients and last_payload:
            await broadcast(last_payload)
        await asyncio.sleep(POLL_INTERVAL)

async def handler(websocket):
    remote = websocket.remote_address
    addr_str = f"{remote[0]}:{remote[1]}"
    
    if addr_str in connected_clients:
        old_ws = connected_clients[addr_str]
        if old_ws.open:
            await old_ws.close()
        del connected_clients[addr_str]
    
    connected_clients[addr_str] = websocket
    print(f"ESP32 已连接 ({addr_str})，当前设备数: {len(connected_clients)}")
    
    try:
        async for message in websocket:
            pass
    except websockets.exceptions.ConnectionClosedError as e:
        print(f"{addr_str} 连接关闭: 码={e.code} 原因={e.reason}")
    except Exception as e:
        print(f"{addr_str} 连接异常: {e}")
    finally:
        if addr_str in connected_clients:
            del connected_clients[addr_str]
        print(f"ESP32 已断开 ({addr_str})，当前设备数: {len(connected_clients)}")

async def process_request(connection, request):
    if 'Connection' not in request.headers:
        request.headers['Connection'] = 'Upgrade'
    if 'Upgrade' not in request.headers:
        request.headers['Upgrade'] = 'websocket'
    if 'Sec-WebSocket-Key' not in request.headers:
        random_bytes = os.urandom(16)
        request.headers['Sec-WebSocket-Key'] = base64.b64encode(random_bytes).decode()
    if 'Sec-WebSocket-Version' not in request.headers:
        request.headers['Sec-WebSocket-Version'] = '13'
    return None

async def main():
    asyncio.create_task(poll_loop())
    async with websockets.serve(
        handler, WS_HOST, WS_PORT,
        process_request=process_request,
        ping_interval=10,      # 每10秒发Ping
        ping_timeout=5         # 5秒没收到Pong就断开
    ):
        print(f"WebSocket 服务运行在 ws://{WS_HOST}:{WS_PORT}")
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
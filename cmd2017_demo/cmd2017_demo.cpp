// cmd2017_demo.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#pragma execution_character_set("utf-8")

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8000
#define MAX_CLIENTS 100
#define BUFFER_SIZE 4096

struct WebSocketFrame {
    bool fin;
    int opcode;
    bool mask;
    uint64_t payloadLength;
    uint32_t maskingKey;
    std::vector<uint8_t> payload;
};

struct ChatUser {
    SOCKET socket;
    std::string nickname;
    int roomId;
    bool isWebSocket;
};

std::mutex clientsMutex;
std::map<SOCKET, ChatUser> clients;
std::map<int, std::vector<SOCKET>> rooms;

std::string htmlPage = R"(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>聊天室</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: 'Microsoft YaHei', Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; display: flex; justify-content: center; align-items: center; padding: 20px; }
        .container { width: 100%; max-width: 800px; background: white; border-radius: 10px; box-shadow: 0 14px 28px rgba(0,0,0,0.25), 0 10px 10px rgba(0,0,0,0.22); overflow: hidden; }
        .login-screen { padding: 40px; text-align: center; }
        .login-screen h1 { color: #667eea; margin-bottom: 30px; font-size: 2.5em; }
        .form-group { margin-bottom: 20px; text-align: left; }
        .form-group label { display: block; margin-bottom: 8px; color: #333; font-weight: bold; }
        .form-group input, .form-group select { width: 100%; padding: 12px; border: 2px solid #ddd; border-radius: 5px; font-size: 16px; transition: border-color 0.3s; }
        .form-group input:focus, .form-group select:focus { outline: none; border-color: #667eea; }
        .btn { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; padding: 15px 30px; border-radius: 5px; font-size: 18px; cursor: pointer; transition: transform 0.2s; }
        .btn:hover { transform: translateY(-2px); }
        .chat-screen { display: none; height: 600px; flex-direction: column; }
        .chat-header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 15px 20px; display: flex; justify-content: space-between; align-items: center; }
        .chat-header h2 { font-size: 1.2em; }
        .room-info { font-size: 0.9em; opacity: 0.9; }
        .chat-messages { flex: 1; overflow-y: auto; padding: 20px; background: #f8f9fa; }
        .message { margin-bottom: 15px; animation: fadeIn 0.3s ease; }
        @keyframes fadeIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }
        .message-header { display: flex; align-items: center; margin-bottom: 5px; }
        .message-nickname { font-weight: bold; color: #667eea; margin-right: 10px; }
        .message-time { font-size: 0.8em; color: #999; }
        .message-content { padding: 10px 15px; border-radius: 18px; display: inline-block; max-width: 70%; word-wrap: break-word; }
        .message.own .message-content { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; }
        .message:not(.own) .message-content { background: white; color: #333; box-shadow: 0 1px 2px rgba(0,0,0,0.1); }
        .message.system .message-content { background: #e8f4f8; color: #31708f; font-style: italic; text-align: center; width: 100%; max-width: 100%; }
        .message-image { max-width: 300px; max-height: 300px; border-radius: 8px; cursor: pointer; }
        .chat-input { padding: 15px; background: white; border-top: 1px solid #eee; display: flex; gap: 10px; align-items: center; }
        .file-input { display: none; }
        .file-label { background: #e9ecef; color: #495057; padding: 10px 15px; border-radius: 5px; cursor: pointer; transition: background 0.2s; }
        .file-label:hover { background: #dde2e6; }
        .text-input { flex: 1; padding: 12px; border: 2px solid #ddd; border-radius: 25px; font-size: 16px; outline: none; transition: border-color 0.2s; }
        .text-input:focus { border-color: #667eea; }
        .send-btn { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; padding: 12px 20px; border-radius: 25px; cursor: pointer; transition: transform 0.2s; }
        .send-btn:hover { transform: scale(1.05); }
        .image-preview { padding: 10px; background: #f8f9fa; border-top: 1px solid #eee; display: none; align-items: center; gap: 10px; }
        .preview-img { max-width: 100px; max-height: 100px; border-radius: 5px; }
        .cancel-btn { background: #dc3545; color: white; border: none; padding: 8px 15px; border-radius: 5px; cursor: pointer; }
    </style>
</head>
<body>
    <div class="container">
        <div class="login-screen" id="loginScreen">
            <h1>聊天室</h1>
            <div class="form-group">
                <label for="nickname">昵称：</label>
                <input type="text" id="nickname" placeholder="请输入您的昵称" maxlength="20">
            </div>
            <div class="form-group">
                <label for="roomId">选择聊天室：</label>
                <select id="roomId">
                    <option value="1">聊天室 1</option>
                    <option value="2">聊天室 2</option>
                    <option value="3">聊天室 3</option>
                    <option value="4">聊天室 4</option>
                    <option value="5">聊天室 5</option>
                </select>
            </div>
            <button class="btn" id="enterBtn">进入聊天室</button>
        </div>
        <div class="chat-screen" id="chatScreen">
            <div class="chat-header">
                <h2 id="headerNickname">用户</h2>
                <div class="room-info" id="roomInfo">聊天室 1</div>
            </div>
            <div class="chat-messages" id="chatMessages"></div>
            <div class="image-preview" id="imagePreview">
                <img class="preview-img" id="previewImg" src="" alt="预览">
                <button class="cancel-btn" id="cancelBtn">取消</button>
            </div>
            <div class="chat-input">
                <input type="file" class="file-input" id="fileInput" accept="image/*">
                <label class="file-label" for="fileInput">图片</label>
                <input type="text" class="text-input" id="messageInput" placeholder="输入消息..." maxlength="500">
                <button class="send-btn" id="sendBtn">发送</button>
            </div>
        </div>
    </div>

    <script>
        let ws = null;
        let currentNickname = '';
        let currentRoomId = 0;
        let selectedImage = null;

        const loginScreen = document.getElementById('loginScreen');
        const chatScreen = document.getElementById('chatScreen');
        const nicknameInput = document.getElementById('nickname');
        const roomIdSelect = document.getElementById('roomId');
        const enterBtn = document.getElementById('enterBtn');
        const headerNickname = document.getElementById('headerNickname');
        const roomInfo = document.getElementById('roomInfo');
        const chatMessages = document.getElementById('chatMessages');
        const messageInput = document.getElementById('messageInput');
        const sendBtn = document.getElementById('sendBtn');
        const fileInput = document.getElementById('fileInput');
        const imagePreview = document.getElementById('imagePreview');
        const previewImg = document.getElementById('previewImg');
        const cancelBtn = document.getElementById('cancelBtn');

        enterBtn.addEventListener('click', enterChat);
        nicknameInput.addEventListener('keypress', (e) => { if (e.key === 'Enter') enterChat(); });
        sendBtn.addEventListener('click', sendMessage);
        messageInput.addEventListener('keypress', (e) => { if (e.key === 'Enter') sendMessage(); });
        fileInput.addEventListener('change', handleFileSelect);
        cancelBtn.addEventListener('click', cancelImage);

        function enterChat() {
            const nickname = nicknameInput.value.trim();
            const roomId = parseInt(roomIdSelect.value);

            if (!nickname) {
                alert('请输入昵称！');
                return;
            }

            currentNickname = nickname;
            currentRoomId = roomId;

            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.host}/ws`;
            ws = new WebSocket(wsUrl);

            ws.onopen = () => {
                const joinMsg = {
                    type: 'join',
                    nickname: nickname,
                    roomId: roomId
                };
                ws.send(JSON.stringify(joinMsg));

                loginScreen.style.display = 'none';
                chatScreen.style.display = 'flex';
                headerNickname.textContent = nickname;
                roomInfo.textContent = `聊天室 ${roomId}`;
            };

            ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    handleMessage(data);
                } catch (e) {
                    console.error('解析消息失败:', e);
                }
            };

            ws.onerror = (error) => {
                console.error('WebSocket错误:', error);
                alert('连接服务器失败，请稍后重试。');
            };

            ws.onclose = () => {
                console.log('WebSocket连接已关闭');
                addSystemMessage('与服务器的连接已断开。');
            };
        }

        function handleMessage(data) {
            if (data.type === 'join') {
                addSystemMessage(`${data.nickname} 进入了聊天室`);
            } else if (data.type === 'leave') {
                addSystemMessage(`${data.nickname} 退出了聊天室`);
            } else if (data.type === 'message') {
                addMessage(data.nickname, data.content, data.timestamp, data.nickname === currentNickname);
            } else if (data.type === 'image') {
                addImageMessage(data.nickname, data.imageData, data.timestamp, data.nickname === currentNickname);
            }
        }

        function addMessage(nickname, content, timestamp, isOwn) {
            const messageDiv = document.createElement('div');
            messageDiv.className = `message ${isOwn ? 'own' : ''}`;
            
            const headerDiv = document.createElement('div');
            headerDiv.className = 'message-header';
            const nicknameSpan = document.createElement('span');
            nicknameSpan.className = 'message-nickname';
            nicknameSpan.textContent = nickname;
            const timeSpan = document.createElement('span');
            timeSpan.className = 'message-time';
            timeSpan.textContent = formatTime(timestamp);
            headerDiv.appendChild(nicknameSpan);
            headerDiv.appendChild(timeSpan);
            
            const contentDiv = document.createElement('div');
            contentDiv.className = 'message-content';
            contentDiv.textContent = content;
            
            messageDiv.appendChild(headerDiv);
            messageDiv.appendChild(contentDiv);
            
            chatMessages.appendChild(messageDiv);
            chatMessages.scrollTop = chatMessages.scrollHeight;
        }

        function addImageMessage(nickname, imageData, timestamp, isOwn) {
            const messageDiv = document.createElement('div');
            messageDiv.className = `message ${isOwn ? 'own' : ''}`;
            
            const headerDiv = document.createElement('div');
            headerDiv.className = 'message-header';
            const nicknameSpan = document.createElement('span');
            nicknameSpan.className = 'message-nickname';
            nicknameSpan.textContent = nickname;
            const timeSpan = document.createElement('span');
            timeSpan.className = 'message-time';
            timeSpan.textContent = formatTime(timestamp);
            headerDiv.appendChild(nicknameSpan);
            headerDiv.appendChild(timeSpan);
            
            const contentDiv = document.createElement('div');
            contentDiv.className = 'message-content';
            const img = document.createElement('img');
            img.className = 'message-image';
            img.src = imageData;
            img.onclick = () => window.open(imageData);
            contentDiv.appendChild(img);
            
            messageDiv.appendChild(headerDiv);
            messageDiv.appendChild(contentDiv);
            
            chatMessages.appendChild(messageDiv);
            chatMessages.scrollTop = chatMessages.scrollHeight;
        }

        function addSystemMessage(content) {
            const messageDiv = document.createElement('div');
            messageDiv.className = 'message system';
            
            const contentDiv = document.createElement('div');
            contentDiv.className = 'message-content';
            contentDiv.textContent = content;
            
            messageDiv.appendChild(contentDiv);
            
            chatMessages.appendChild(messageDiv);
            chatMessages.scrollTop = chatMessages.scrollHeight;
        }

        function formatTime(timestamp) {
            const date = new Date(timestamp);
            const hours = String(date.getHours()).padStart(2, '0');
            const minutes = String(date.getMinutes()).padStart(2, '0');
            const seconds = String(date.getSeconds()).padStart(2, '0');
            return `${hours}:${minutes}:${seconds}`;
        }

        function sendMessage() {
            if (selectedImage) {
                const msg = {
                    type: 'image',
                    nickname: currentNickname,
                    roomId: currentRoomId,
                    imageData: selectedImage
                };
                ws.send(JSON.stringify(msg));
                cancelImage();
            } else {
                const content = messageInput.value.trim();
                if (content && ws && ws.readyState === WebSocket.OPEN) {
                    const msg = {
                        type: 'message',
                        nickname: currentNickname,
                        roomId: currentRoomId,
                        content: content
                    };
                    ws.send(JSON.stringify(msg));
                    messageInput.value = '';
                }
            }
        }

        function handleFileSelect(e) {
            const file = e.target.files[0];
            if (file) {
                if (file.size > 5 * 1024 * 1024) {
                    alert('图片大小不能超过5MB！');
                    return;
                }
                
                const reader = new FileReader();
                reader.onload = (event) => {
                    selectedImage = event.target.result;
                    previewImg.src = selectedImage;
                    imagePreview.style.display = 'flex';
                };
                reader.readAsDataURL(file);
            }
        }

        function cancelImage() {
            selectedImage = null;
            previewImg.src = '';
            imagePreview.style.display = 'none';
            fileInput.value = '';
        }

        window.addEventListener('beforeunload', () => {
            if (ws) {
                ws.close();
            }
        });
    </script>
</body>
</html>
)";

std::string base64Encode(const std::vector<uint8_t>& data) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int i = 0;
    uint32_t arr3 = 0;
    
    for (uint8_t byte : data) {
        arr3 |= (byte << (16 - i * 8));
        i++;
        if (i == 3) {
            result += chars[(arr3 >> 18) & 0x3F];
            result += chars[(arr3 >> 12) & 0x3F];
            result += chars[(arr3 >> 6) & 0x3F];
            result += chars[arr3 & 0x3F];
            i = 0;
            arr3 = 0;
        }
    }
    
    if (i > 0) {
        result += chars[(arr3 >> 18) & 0x3F];
        result += chars[(arr3 >> 12) & 0x3F];
        if (i == 2) {
            result += chars[(arr3 >> 6) & 0x3F];
            result += '=';
        } else {
            result += "==";
        }
    }
    
    return result;
}

std::vector<uint8_t> sha1(const std::string& data) {
    std::vector<uint8_t> result(20, 0);
    
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (BYTE*)data.c_str(), (DWORD)data.length(), 0)) {
                DWORD hashLen = 20;
                CryptGetHashParam(hHash, HP_HASHVAL, result.data(), &hashLen, 0);
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    
    return result;
}

std::string generateWebSocketAcceptKey(const std::string& clientKey) {
    const std::string magicString = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = clientKey + magicString;
    std::vector<uint8_t> sha1Result = sha1(combined);
    return base64Encode(sha1Result);
}

bool parseWebSocketFrame(const std::vector<uint8_t>& buffer, WebSocketFrame& frame) {
    if (buffer.size() < 2) return false;
    
    frame.fin = (buffer[0] & 0x80) != 0;
    frame.opcode = buffer[0] & 0x0F;
    frame.mask = (buffer[1] & 0x80) != 0;
    frame.payloadLength = buffer[1] & 0x7F;
    
    size_t offset = 2;
    
    if (frame.payloadLength == 126) {
        if (buffer.size() < 4) return false;
        frame.payloadLength = (buffer[2] << 8) | buffer[3];
        offset = 4;
    } else if (frame.payloadLength == 127) {
        if (buffer.size() < 10) return false;
        frame.payloadLength = 0;
        for (int i = 0; i < 8; i++) {
            frame.payloadLength |= (uint64_t)buffer[2 + i] << (8 * (7 - i));
        }
        offset = 10;
    }
    
    if (frame.mask) {
        if (buffer.size() < offset + 4) return false;
        frame.maskingKey = (buffer[offset] << 24) | (buffer[offset + 1] << 16) | 
                           (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;
    }
    
    if (buffer.size() < offset + frame.payloadLength) return false;
    
    frame.payload = std::vector<uint8_t>(buffer.begin() + offset, buffer.begin() + offset + frame.payloadLength);
    
    if (frame.mask) {
        for (size_t i = 0; i < frame.payload.size(); i++) {
            frame.payload[i] ^= (frame.maskingKey >> (8 * (3 - (i % 4)))) & 0xFF;
        }
    }
    
    return true;
}

std::vector<uint8_t> buildWebSocketFrame(const std::string& message, int opcode = 1) {
    std::vector<uint8_t> frame;
    
    frame.push_back(0x80 | opcode);
    
    size_t messageLength = message.length();
    if (messageLength <= 125) {
        frame.push_back((uint8_t)messageLength);
    } else if (messageLength <= 65535) {
        frame.push_back(126);
        frame.push_back((messageLength >> 8) & 0xFF);
        frame.push_back(messageLength & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back((messageLength >> (8 * i)) & 0xFF);
        }
    }
    
    frame.insert(frame.end(), message.begin(), message.end());
    
    return frame;
}

void sendWebSocketMessage(SOCKET socket, const std::string& message) {
    std::vector<uint8_t> frame = buildWebSocketFrame(message);
    send(socket, (char*)frame.data(), (int)frame.size(), 0);
}

std::string getCurrentTimestamp() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    char buf[32];
    localtime_s(&timeinfo, &now);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    return std::string(buf);
}

void broadcastToRoom(int roomId, const std::string& message, SOCKET excludeSocket = INVALID_SOCKET) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    
    if (rooms.find(roomId) == rooms.end()) return;
    
    for (SOCKET socket : rooms[roomId]) {
        if (socket != excludeSocket && clients.find(socket) != clients.end()) {
            if (clients[socket].isWebSocket) {
                sendWebSocketMessage(socket, message);
            }
        }
    }
}

void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    std::string request;
    bool isWebSocket = false;
    ChatUser user;
    user.socket = clientSocket;
    user.isWebSocket = false;
    user.roomId = 0;
    
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytesReceived <= 0) {
            break;
        }
        
        buffer[bytesReceived] = '\0';
        
        if (!isWebSocket) {
            request += buffer;
            
            if (request.find("\r\n\r\n") != std::string::npos) {
                std::istringstream requestStream(request);
                std::string method, path, protocol;
                requestStream >> method >> path >> protocol;
                
                if (method == "GET" && path == "/ws") {
                    size_t keyPos = request.find("Sec-WebSocket-Key: ");
                    if (keyPos != std::string::npos) {
                        keyPos += 19;
                        size_t keyEnd = request.find("\r\n", keyPos);
                        std::string clientKey = request.substr(keyPos, keyEnd - keyPos);
                        std::string acceptKey = generateWebSocketAcceptKey(clientKey);
                        
                        std::string response = 
                            "HTTP/1.1 101 Switching Protocols\r\n"
                            "Upgrade: websocket\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Accept: " + acceptKey + "\r\n"
                            "\r\n";
                        
                        send(clientSocket, response.c_str(), (int)response.length(), 0);
                        isWebSocket = true;
                        user.isWebSocket = true;
                        
                        {
                            std::lock_guard<std::mutex> lock(clientsMutex);
                            clients[clientSocket] = user;
                        }
                    }
                } else if (method == "GET" && (path == "/" || path == "/index.html")) {
                    std::string response = 
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html; charset=utf-8\r\n"
                        "Content-Length: " + std::to_string(htmlPage.length()) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n" + htmlPage;
                    
                    send(clientSocket, response.c_str(), (int)response.length(), 0);
                    closesocket(clientSocket);
                    return;
                } else {
                    std::string response = 
                        "HTTP/1.1 404 Not Found\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: 13\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "404 Not Found";
                    
                    send(clientSocket, response.c_str(), (int)response.length(), 0);
                    closesocket(clientSocket);
                    return;
                }
                
                request.clear();
            }
        } else {
            std::vector<uint8_t> frameBuffer(buffer, buffer + bytesReceived);
            WebSocketFrame frame;
            
            if (parseWebSocketFrame(frameBuffer, frame)) {
                if (frame.opcode == 0x8) {
                    break;
                } else if (frame.opcode == 0x1 || frame.opcode == 0x2) {
                    std::string message(frame.payload.begin(), frame.payload.end());
                    
                    try {
                        size_t jsonStart = message.find('{');
                        if (jsonStart != std::string::npos) {
                            std::string jsonStr = message.substr(jsonStart);
                            
                            size_t typePos = jsonStr.find("\"type\"");
                            if (typePos != std::string::npos) {
                                size_t typeValStart = jsonStr.find('"', typePos + 7);
                                size_t typeValEnd = jsonStr.find('"', typeValStart + 1);
                                std::string type = jsonStr.substr(typeValStart + 1, typeValEnd - typeValStart - 1);
                                
                                if (type == "join") {
                                    size_t nicknamePos = jsonStr.find("\"nickname\"");
                                    size_t roomIdPos = jsonStr.find("\"roomId\"");
                                    
                                    if (nicknamePos != std::string::npos && roomIdPos != std::string::npos) {
                                        size_t nickValStart = jsonStr.find('"', nicknamePos + 11);
                                        size_t nickValEnd = jsonStr.find('"', nickValStart + 1);
                                        std::string nickname = jsonStr.substr(nickValStart + 1, nickValEnd - nickValStart - 1);
                                        
                                        size_t roomIdValStart = jsonStr.find(':', roomIdPos) + 1;
                                        size_t roomIdValEnd = jsonStr.find_first_of(",}", roomIdValStart);
                                        int roomId = std::stoi(jsonStr.substr(roomIdValStart, roomIdValEnd - roomIdValStart));
                                        
                                        if (roomId >= 1 && roomId <= 5 && !nickname.empty()) {
                                            std::lock_guard<std::mutex> lock(clientsMutex);
                                            
                                            if (clients.find(clientSocket) != clients.end()) {
                                                int oldRoomId = clients[clientSocket].roomId;
                                                if (oldRoomId > 0 && rooms.find(oldRoomId) != rooms.end()) {
                                                    auto& roomUsers = rooms[oldRoomId];
                                                    roomUsers.erase(std::remove(roomUsers.begin(), roomUsers.end(), clientSocket), roomUsers.end());
                                                }
                                            }
                                            
                                            clients[clientSocket].nickname = nickname;
                                            clients[clientSocket].roomId = roomId;
                                            rooms[roomId].push_back(clientSocket);
                                            
                                            std::string joinMsg = 
                                                "{\"type\":\"join\",\"nickname\":\"" + nickname + "\","
                                                "\"timestamp\":\"" + getCurrentTimestamp() + "\"}";
                                            
                                            for (SOCKET socket : rooms[roomId]) {
                                                if (socket != clientSocket && clients.find(socket) != clients.end()) {
                                                    if (clients[socket].isWebSocket) {
                                                        sendWebSocketMessage(socket, joinMsg);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else if (type == "message") {
                                    size_t nicknamePos = jsonStr.find("\"nickname\"");
                                    size_t roomIdPos = jsonStr.find("\"roomId\"");
                                    size_t contentPos = jsonStr.find("\"content\"");
                                    
                                    if (nicknamePos != std::string::npos && roomIdPos != std::string::npos && contentPos != std::string::npos) {
                                        size_t nickValStart = jsonStr.find('"', nicknamePos + 11);
                                        size_t nickValEnd = jsonStr.find('"', nickValStart + 1);
                                        std::string nickname = jsonStr.substr(nickValStart + 1, nickValEnd - nickValStart - 1);
                                        
                                        size_t roomIdValStart = jsonStr.find(':', roomIdPos) + 1;
                                        size_t roomIdValEnd = jsonStr.find_first_of(",}", roomIdValStart);
                                        int roomId = std::stoi(jsonStr.substr(roomIdValStart, roomIdValEnd - roomIdValStart));
                                        
                                        size_t contentValStart = jsonStr.find('"', contentPos + 10);
                                        size_t contentValEnd = jsonStr.find('"', contentValStart + 1);
                                        std::string content = jsonStr.substr(contentValStart + 1, contentValEnd - contentValStart - 1);
                                        
                                        if (!content.empty() && content.length() <= 500) {
                                            std::string msg = 
                                                "{\"type\":\"message\",\"nickname\":\"" + nickname + "\","
                                                "\"content\":\"" + content + "\","
                                                "\"timestamp\":\"" + getCurrentTimestamp() + "\"}";
                                            
                                            broadcastToRoom(roomId, msg);
                                        }
                                    }
                                } else if (type == "image") {
                                    size_t nicknamePos = jsonStr.find("\"nickname\"");
                                    size_t roomIdPos = jsonStr.find("\"roomId\"");
                                    size_t imageDataPos = jsonStr.find("\"imageData\"");
                                    
                                    if (nicknamePos != std::string::npos && roomIdPos != std::string::npos && imageDataPos != std::string::npos) {
                                        size_t nickValStart = jsonStr.find('"', nicknamePos + 11);
                                        size_t nickValEnd = jsonStr.find('"', nickValStart + 1);
                                        std::string nickname = jsonStr.substr(nickValStart + 1, nickValEnd - nickValStart - 1);
                                        
                                        size_t roomIdValStart = jsonStr.find(':', roomIdPos) + 1;
                                        size_t roomIdValEnd = jsonStr.find_first_of(",}", roomIdValStart);
                                        int roomId = std::stoi(jsonStr.substr(roomIdValStart, roomIdValEnd - roomIdValStart));
                                        
                                        size_t imageDataValStart = jsonStr.find('"', imageDataPos + 12);
                                        size_t imageDataValEnd = jsonStr.find('"', imageDataValStart + 1);
                                        std::string imageData = jsonStr.substr(imageDataValStart + 1, imageDataValEnd - imageDataValStart - 1);
                                        
                                        if (!imageData.empty() && imageData.length() <= 7 * 1024 * 1024) {
                                            std::string msg = 
                                                "{\"type\":\"image\",\"nickname\":\"" + nickname + "\","
                                                "\"imageData\":\"" + imageData + "\","
                                                "\"timestamp\":\"" + getCurrentTimestamp() + "\"}";
                                            
                                            broadcastToRoom(roomId, msg);
                                        }
                                    }
                                }
                            }
                        }
                    } catch (...) {
                    }
                }
            }
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        
        if (clients.find(clientSocket) != clients.end()) {
            ChatUser& leavingUser = clients[clientSocket];
            
            if (leavingUser.roomId > 0 && !leavingUser.nickname.empty()) {
                std::string leaveMsg = 
                    "{\"type\":\"leave\",\"nickname\":\"" + leavingUser.nickname + "\","
                    "\"timestamp\":\"" + getCurrentTimestamp() + "\"}";
                
                if (rooms.find(leavingUser.roomId) != rooms.end()) {
                    for (SOCKET socket : rooms[leavingUser.roomId]) {
                        if (socket != clientSocket && clients.find(socket) != clients.end()) {
                            if (clients[socket].isWebSocket) {
                                sendWebSocketMessage(socket, leaveMsg);
                            }
                        }
                    }
                    
                    auto& roomUsers = rooms[leavingUser.roomId];
                    roomUsers.erase(std::remove(roomUsers.begin(), roomUsers.end(), clientSocket), roomUsers.end());
                }
            }
            
            clients.erase(clientSocket);
        }
    }
    
    closesocket(clientSocket);
}

int main()
{
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup 失败: " << iResult << std::endl;
        return 1;
    }
    
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cout << "创建 socket 失败: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);
    
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "绑定端口失败: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "监听失败: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    
    std::cout << "聊天室服务器已启动，监听端口: " << PORT << std::endl;
    std::cout << "请在浏览器中访问: http://localhost:" << PORT << std::endl;
    std::cout << "按 Ctrl+C 停止服务器" << std::endl;
    
    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cout << "接受连接失败: " << WSAGetLastError() << std::endl;
            continue;
        }
        
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }
    
    closesocket(serverSocket);
    WSACleanup();
    
    return 0;
}

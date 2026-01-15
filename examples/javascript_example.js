// Example: Using FrancoDB Client in Node.js (Packet Protocol)
const net = require('net');

// Protocol Constants
const CMD_TEXT = 'Q'.charCodeAt(0);
const CMD_JSON = 'J'.charCodeAt(0);

class FrancoDBClient {
    constructor(host = 'localhost', port = 2501) {
        this.host = host;
        this.port = port;
        this.socket = null;
        this.connected = false;
    }

    connect(username = '', password = '', database = '') {
        return new Promise((resolve, reject) => {
            this.socket = new net.Socket();

            this.socket.connect(this.port, this.host, async () => {
                this.connected = true;

                // Login
                if (username && password) {
                    const res = await this.query(`LOGIN ${username} ${password};\n`);
                    if (!res.includes('LOGIN OK')) {
                        this.disconnect();
                        return reject(new Error('Login failed'));
                    }
                }

                // Use DB
                if (database) {
                    await this.query(`USE ${database};\n`);
                }
                resolve(true);
            });

            this.socket.on('error', (err) => reject(err));
        });
    }

    query(sql, mode = 'text') {
        return new Promise((resolve, reject) => {
            if (!this.connected) return reject(new Error('Not connected'));

            // 1. Determine Type
            let msgType = CMD_TEXT;
            if (mode === 'json') msgType = CMD_JSON;

            // 2. Prepare Buffers
            const payload = Buffer.from(sql, 'utf-8');
            const header = Buffer.alloc(5);

            // 3. Write Header
            header.writeUInt8(msgType, 0);       // Byte 0: Type
            header.writeUInt32BE(payload.length, 1); // Byte 1-4: Length (Big Endian)

            // 4. Send Packet
            const packet = Buffer.concat([header, payload]);

            // 5. Wait for Response (Simple one-shot listener)
            const onData = (data) => {
                this.socket.removeListener('data', onData);
                resolve(data.toString().trim());
            };

            this.socket.on('data', onData);
            this.socket.write(packet);
        });
    }

    disconnect() {
        if (this.socket) {
            this.socket.end();
            this.connected = false;
        }
    }
}

// Example usage
async function main() {
    const client = new FrancoDBClient('localhost', 2501);

    try {
        await client.connect('maayn', 'root', 'mydb');
        console.log('Connected!');

        // JSON Query
        const jsonStr = await client.query('2e5tar * men users;', 'json');
        console.log('JSON Output:', jsonStr);

        // Parse it if valid JSON
        try {
            const jsonObj = JSON.parse(jsonStr);
            console.log('Parsed Rows:', jsonObj.data?.rows);
        } catch(e) {}

        client.disconnect();
    } catch (error) {
        console.error('Error:', error);
    }
}

main();
"""Run: python3 -m tools.espnow_linux --interface espnowmon"""
import argparse
import json
import logging
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from .radio import MonitorRadio
from .state import SensorState


def handler_for(state):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path not in ('/', '/sensors'):
                self.send_error(404)
                return
            body = json.dumps(state.snapshot(), ensure_ascii=False).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, format, *args):
            logging.info('http: ' + format, *args)
    return Handler


def main():
    parser = argparse.ArgumentParser(description='Linux ESP-NOW master for espnow-sensors-node')
    parser.add_argument('--interface', required=True, help='existing 2.4 GHz monitor interface')
    parser.add_argument('--host', default='127.0.0.1', help='HTTP bind address (default 127.0.0.1)')
    parser.add_argument('--port', default=8765, type=int, help='HTTP port (default 8765)')
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(message)s')
    radio = MonitorRadio(args.interface)
    try:
        state = SensorState()
        server = ThreadingHTTPServer((args.host, args.port), handler_for(state))
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        logging.info('monitor %s, MAC %s; HTTP http://%s:%d/sensors', args.interface,
                     ':'.join(f'{b:02x}' for b in radio.mac), args.host, server.server_port)
        next_heartbeat = 0.0
        try:
            while True:
                now = time.monotonic()
                if now >= next_heartbeat:
                    radio.send_heartbeat()
                    next_heartbeat = now + 0.5
                received = radio.receive()
                if received:
                    mac, event = received
                    state.update(mac, event)
                    if event['kind'] != 'alive':
                        logging.info('%s %s', mac, json.dumps(event, ensure_ascii=False))
        except KeyboardInterrupt:
            logging.info('stopping')
        finally:
            server.shutdown()
            server.server_close()
            worker.join()
    finally:
        radio.close()


if __name__ == '__main__':
    main()

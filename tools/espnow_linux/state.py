"""Thread-safe sensor state, using local monotonic time for freshness."""
import threading
import time


class SensorState:
    def __init__(self, stale_seconds=45, sensor_stale_seconds=300, clock=time.monotonic):
        self._nodes = {}
        self._lock = threading.Lock()
        self._stale = stale_seconds
        self._sensor_stale = sensor_stale_seconds
        self._clock = clock

    def update(self, mac, event):
        now = self._clock()
        with self._lock:
            node = self._nodes.setdefault(mac, {'last_seen': now})
            node['last_seen'] = now
            kind = event['kind']
            if kind in ('identity', 'climate', 'presence'):
                node[kind] = (now, {k: v for k, v in event.items() if k != 'kind'})

    def snapshot(self):
        now = self._clock()
        with self._lock:
            nodes = []
            for mac, data in self._nodes.items():
                node = {'mac': mac, 'online': now - data['last_seen'] <= self._stale,
                        'last_seen_seconds_ago': round(now - data['last_seen'], 1)}
                for kind in ('identity', 'climate', 'presence'):
                    if kind in data:
                        timestamp, values = data[kind]
                        fresh = (now - timestamp < self._sensor_stale if kind in ('climate', 'presence')
                                 else now - timestamp <= self._stale)
                        node[kind] = {**values, 'fresh': fresh,
                                      'age_seconds': round(now - timestamp, 1)}
                nodes.append(node)
            return {'nodes': nodes}

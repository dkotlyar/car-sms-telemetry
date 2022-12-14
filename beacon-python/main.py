import base64
import json
import math
import os
import re
import time
from datetime import datetime, timedelta

import cv2
import psycopg2
import psycopg2.extras
import pytz
import serial
from pymodbus.client.sync import ModbusSerialClient as ModbusClient

from codesys_utils import PT

MAXIMUM_HTTP_PAYLOAD_SIZE = 30 * (2**10)

dbname = os.getenv('DB_NAME', 'beacon-local')
username = os.getenv('DB_USER', 'postgres')
passwd = os.getenv('DB_PASSWD', 'qwerty')
host = os.getenv('DB_HOST', 'localhost')
port = int(os.getenv('DB_PORT', 5432))


class SIM868_HttpRequest:
    def __init__(self):
        self.url = None
        self.payload = None
        self.context = None
        self.status = 0

    def post(self, url, payload, context):
        if len(payload) > MAXIMUM_HTTP_PAYLOAD_SIZE:  # maximum SIM868 POST payload size
            payload = payload[:MAXIMUM_HTTP_PAYLOAD_SIZE]
        self.url = url
        self.payload = payload
        self.context = context
        self.status = 0

class SIM868:
    def __init__(self, port, baudrate=9600, parity=serial.PARITY_NONE, stopbits=1, bytesize=8, timeout=1, dbcred=None):
        assert dbcred is not None
        dbname, username, passwd, host, dbport = dbcred
        self.dbcon = psycopg2.connect(dbname=dbname, user=username, password=passwd, host=host, port=dbport)
        self.db_snapshots = 'beacon_local_snapshot'
        self.db_media = 'beacon_local_media'

        try:
            self.port = serial.Serial(port=port, baudrate=baudrate, parity=parity,
                                      stopbits=stopbits, bytesize=bytesize, timeout=timeout)
        except Exception:
            self.port = None

        self.cgnsurc = 2

        self.request = SIM868_HttpRequest()
        self.telemetry_url = 'http://dkotlyar.ru:8000/telemetry'
        self.media_url = 'http://dkotlyar.ru:8000/media'

        self.rx_buffer = ''
        self.tx_buffer = []
        self.loop_state = 'INIT'
        self._data_send_cycle = 'GET_SNAPSHOTS'
        self.http_state = 'INIT'
        self.media_state = 'INIT'
        self.http_timeout_ton = PT()
        self.media_timeout_ton = PT()
        self.status = 'OK'
        self.lastcommand_timeout = PT()
        self.imei = ''
        self.gnss = ''
        self.snapshots = []  # ?????????? ?????? ???????????????? ?????????????????? ???? ????????????
        self.media = None  # ?????????? ?????? ???????????????? ?????????? ???? ????????????

    def put(self, msg):
        write = f"{msg}\r\n".encode('utf-8')
        print(f"$ {msg}")
        self.port.write(write)
        self.lastcommand_timeout.reset()

    def handle(self, msg):
        print(f"> {msg}")

        if msg == 'ERROR':
            self.status = 'ERROR'
        elif msg == 'OK':
            self.status = 'OK'
        elif msg == '+CPIN: READY':
            self.loop_state = 'CPIN_READY'
        elif msg == 'NORMAL POWER DOWN':
            pass  # TODO:
        elif msg.startswith('+UGNSINF: '):
            self.gnss = msg[10:]
        elif msg.startswith('+HTTPACTION: '):
            type, code, len = list(map(int, msg[13:].split(',')))
            type_str = ['GET', 'POST'][type]
            print(f'{type_str} HTTP{code}')
            if code == 200:
                self.http_state = 'HTTP_OK'
            else:
                self.http_state = 'HTTP_NETWORK_ERROR'
        elif msg == 'DOWNLOAD':
            self.status = 'OK'
        else:
            if self.status == 'WAIT_IMEI':
                self.imei = msg
                self.busy()

    def powerdown(self):
        if self.port is None:
            return
        self.put('AT+CPOWD=1')

    def communication(self):
        if self.port is None:
            return

        rec = self.port.read_all().decode('utf-8')
        rec = self.rx_buffer + rec
        full = re.fullmatch(r'.*[\r\n]$', rec, re.MULTILINE | re.DOTALL)
        rx_buffer = list(filter(None, re.split(r'[\r\n]', rec)))
        if not full and len(rx_buffer) > 0:
            self.rx_buffer = rx_buffer[-1]
            rx_buffer = rx_buffer[:-1]
        else:
            self.rx_buffer = ''

        for msg in rx_buffer:
            self.handle(msg)

        for msg in self.tx_buffer:
            self.put(msg)
        self.tx_buffer = []

    def busy(self):
        self.status = 'BUSY'

    def is_busy(self):
        return self.status != 'OK' and self.status != 'ERROR'

    def http_loop(self):
        if self.http_state == 'INIT':
            if self.request.url is not None:
                self.http_state = 'SAPBR_INIT'
        elif self.http_state == 'SAPBR_INIT':
            if not self.is_busy():
                self.tx_buffer.append('AT+SAPBR=1,1')
                self.busy()
                self.http_state = 'HTTP_INIT'
        elif self.http_state == 'HTTP_INIT':
            if not self.is_busy():
                self.tx_buffer.append('AT+HTTPINIT')
                self.busy()
                self.http_state = 'PARA_URL'
        elif self.http_state == 'PARA_URL':
            if not self.is_busy():
                if self.status == 'ERROR':
                    self.http_state = 'HTTP_NETWORK_ERROR'
                else:
                    self.tx_buffer.append(f'AT+HTTPPARA=URL,{self.request.url}')
                    self.busy()
                    self.http_state = 'HTTP_DATA'
        elif self.http_state == 'HTTP_DATA':
            if not self.is_busy():
                if self.status == 'ERROR':
                    self.http_state = 'HTTP_NETWORK_ERROR'
                else:
                    self.tx_buffer.append(f'AT+HTTPDATA={len(self.request.payload)},30000')
                    self.busy()
                    self.http_state = 'HTTP_DOWNLOAD'
        elif self.http_state == 'HTTP_DOWNLOAD':
            if not self.is_busy():
                if self.status == 'ERROR':
                    self.http_state = 'HTTP_NETWORK_ERROR'
                else:
                    self.tx_buffer.append(self.request.payload)
                    self.busy()
                    self.http_state = 'HTTP_ACTION'
        elif self.http_state == 'HTTP_ACTION':
            if not self.is_busy():
                if self.status == 'ERROR':
                    self.http_state = 'HTTP_NETWORK_ERROR'
                else:
                    self.tx_buffer.append('AT+HTTPACTION=1')
                    self.busy()
                    self.http_state = 'HTTP_PENDING'
                    self.http_timeout_ton.reset()
        elif self.http_state == 'HTTP_PENDING':
            if not self.is_busy():
                if self.status == 'ERROR':
                    self.http_state = 'HTTP_NETWORK_ERROR'
                if self.http_timeout_ton.ton_reset(True, 120000):
                    self.http_state = 'HTTP_NETWORK_ERROR'
        elif self.http_state == 'HTTP_OK':
            self.tx_buffer.append('AT+HTTPTERM')
            self.busy()
            self.http_state = 'INIT'
            self.request.status = 200
            self.request.url = None
        elif self.http_state == 'HTTP_NETWORK_ERROR':
            self.tx_buffer.append('AT+HTTPTERM')
            self.busy()
            self.http_state = 'INIT'
            self.request.status = 900
            self.request.url = None
        else:
            print(f'Unknown http state: {self.http_state}')
            self.http_state = 'INIT'

    def data_send_cycle(self):
        if self._data_send_cycle == 'SNAPSHOTS':
            snapshots = self.get_snapshots(20)
            if len(snapshots) > 0:
                self.request.post(self.telemetry_url, json.dumps(snapshots), (snapshots, ))
                self._data_send_cycle = 'PENDING_SNAPSHOTS'
            else:
                self._data_send_cycle = 'MEDIA'
        elif self._data_send_cycle == 'PENDING_SNAPSHOTS':
            if self.request.status == 200:
                snapshots, = self.request.context
                self.mark_snapshots(snapshots)
                self._data_send_cycle = 'MEDIA'
            elif self.request.status > 0:
                self._data_send_cycle = 'MEDIA'

        elif self._data_send_cycle == 'MEDIA':
            media = self.get_media()
            if media is not None:
                parts = media['parts']
                part = media['part']
                ext = os.path.splitext(media['filename'])[1][1:]
                timestamp = int(media['timestamp'])
                self.request.post(f'{self.media_url}/{self.imei}/{timestamp}/{ext}/{parts}/{part}', media['payload'], (media, ))
                self._data_send_cycle = 'PENDING_MEDIA'
            else:
                self._data_send_cycle = 'SNAPSHOTS'
        elif self._data_send_cycle == 'PENDING_MEDIA':
            if self.request.status == 200:
                media, = self.request.context
                self.mark_media(media)
                self._data_send_cycle = 'SNAPSHOTS'
            elif self.request.status > 0:
                self._data_send_cycle = 'SNAPSHOTS'
        else:
            self._data_send_cycle = 'SNAPSHOTS'

    def loop(self):
        self.communication()

        if self.lastcommand_timeout.ton_reset(True, 5000) and self.is_busy():
            self.status = 'ERROR'

        if self.loop_state == 'INIT':
            self.tx_buffer.append('AT+CPIN?')
            self.busy()
            self.loop_state = 'CPIN_AWAIT'
        elif self.loop_state == 'CPIN_AWAIT':
            pass
        elif self.loop_state == 'CPIN_READY':
            self.loop_state = 'GET_IMEI'
        elif self.loop_state == 'GET_IMEI':
            if not self.is_busy():
                self.tx_buffer.append('AT+GSN')
                self.status = 'WAIT_IMEI'
                self.loop_state = 'ENABLE_GNSS_1'
        elif self.loop_state == 'ENABLE_GNSS_1':
            if not self.is_busy():
                self.tx_buffer.append('AT+CGNSPWR=1')
                self.busy()
                self.loop_state = 'ENABLE_GNSS_2'
        elif self.loop_state == 'ENABLE_GNSS_2':
            if not self.is_busy():
                self.tx_buffer.append(f'AT+CGNSURC={self.cgnsurc}')
                self.busy()
                self.loop_state = 'OK'
        elif self.loop_state == 'OK':
            self.data_send_cycle()
            self.http_loop()
        else:
            print(f'Incorrect loop state: {self.loop_state}')
            self.loop_state = 'INIT'

    def get_unpublished_snapshots(self):
        cursor = self.dbcon.cursor()
        cursor.execute(f"select count(*) from {self.db_snapshots} where published=false")
        res = cursor.fetchone()
        cursor.close()
        return int(res[0])

    def get_snapshots(self, limit=3):
        cursor = self.dbcon.cursor(cursor_factory=psycopg2.extras.DictCursor)
        cursor.execute(f"select * from {self.db_snapshots} where published=false and session_datetime is not null limit %s", (limit, ))
        res = cursor.fetchall()
        cursor.close()
        vl = lambda v: int(v.timestamp()*1000) if type(v) == datetime else v
        return [{k:vl(v) for k, v in record.items()} for record in res]

    def add_snapshot(self, snapshot):
        cursor = self.dbcon.cursor()
        cursor.execute(f"insert into {self.db_snapshots} (imei, gnss, runtime, distance, vehicle_kmh, engine_rpm, "
                       "snapshot_datetime, session_datetime, obd2_datetime, mcu_millis, published) "
                       "values (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, false) "
                       "RETURNING id ",
                       (snapshot['imei'], snapshot['gnss'], snapshot['run_time'], snapshot['distance'],
                        snapshot['vehicle_kmh'], snapshot['engine_rpm'], datetime.now(tz=pytz.utc),
                        snapshot['session_datetime'], snapshot['obd2_datetime'], snapshot['mcu_millis']))
        snapshot_id = cursor.fetchone()[0]
        self.dbcon.commit()
        cursor.close()
        return snapshot_id

    def mark_snapshots(self, snapshots):
        if len(snapshots) == 0:
            return

        cursor = self.dbcon.cursor()
        if len(snapshots) == 1:
            cursor.execute(f"update {self.db_snapshots} set published=true where id=%s", (snapshots[0]['id'],))
        else:
            ids = tuple(map(lambda x: x['id'], snapshots))
            cursor.execute(f"update {self.db_snapshots} set published=true where id in %s", (ids, ))
        self.dbcon.commit()
        cursor.close()

    def clear_snapshots(self):
        cursor = self.dbcon.cursor(cursor_factory=psycopg2.extras.DictCursor)
        cursor.execute(f"delete from {self.db_snapshots} where (published=true and snapshot_datetime<%s) or "
                       f"(snapshot_datetime<%s)",
                       (datetime.now() - timedelta(days=3), datetime.now() - timedelta(days=7)))
        self.dbcon.commit()
        cursor.close()

    def clear_null_session(self):
        cursor = self.dbcon.cursor()
        cursor.execute(f"delete from {self.db_snapshots} where session_datetime is null")
        self.dbcon.commit()
        cursor.close()

    def update_null_session(self, session_datetime):
        cursor = self.dbcon.cursor()
        cursor.execute(f"update {self.db_snapshots} set session_datetime=%s where session_datetime is null",
                       session_datetime)
        self.dbcon.commit()
        cursor.close()

    def get_media(self):
        cursor = self.dbcon.cursor(cursor_factory=psycopg2.extras.DictCursor)
        cursor.execute(f"select * from {self.db_media} where published<parts order by published desc limit 1")
        res = cursor.fetchone()
        cursor.close()
        if res is None:
            return None
        vl = lambda v: v.timestamp() if type(v) == datetime else v
        media = {k:vl(v) for k, v in res.items()}
        with open(media['filename'], 'rb') as f:
            b64 = base64.b64encode(f.read()).decode('utf-8')
            i = media['published']
            b64_cut = b64[MAXIMUM_HTTP_PAYLOAD_SIZE * i: MAXIMUM_HTTP_PAYLOAD_SIZE * (i + 1)]
            media['part'] = media['published']
            media['payload'] = b64_cut
            if len(b64_cut) == 0:
                new_parts = math.ceil(len(b64) / MAXIMUM_HTTP_PAYLOAD_SIZE)
                cursor = self.dbcon.cursor()
                cursor.execute(f"update {self.db_media} set published=%s, parts=%s where id=%s",
                               (new_parts, new_parts, media['id']))
                cursor.close()
                return self.get_media()
        return media

    def add_media(self, filename, timestamp):
        parts = 0
        with open(filename, 'rb') as f:
            b64 = base64.b64encode(f.read())
            parts = math.ceil(len(b64) / MAXIMUM_HTTP_PAYLOAD_SIZE)

        cursor = self.dbcon.cursor()
        cursor.execute(f"insert into {self.db_media} (filename, timestamp, create_datetime, published, parts) "
                       "values (%s, %s, %s, 0, %s) "
                       "RETURNING id ",
                       (filename, timestamp, datetime.now(tz=pytz.utc), parts))
        media_id = cursor.fetchone()[0]
        self.dbcon.commit()
        cursor.close()
        return media_id

    def mark_media(self, media):
        if media is None:
            return
        cursor = self.dbcon.cursor()
        cursor.execute(f"update {self.db_media} set published=published+1 where id=%s", (media['id'],))
        self.dbcon.commit()
        cursor.close()

    def clear_media(self):
        cursor = self.dbcon.cursor(cursor_factory=psycopg2.extras.DictCursor)
        cursor.execute(f"select * from {self.db_media} where (published=parts and create_datetime<%s) or "
                       f"(create_datetime<%s)",
                       (datetime.now() - timedelta(days=3), datetime.now() - timedelta(days=7)))
        res = cursor.fetchall()
        cursor.close()
        if res is None:
            return
        vl = lambda v: v.timestamp() if type(v) == datetime else v
        media = [{k:vl(v) for k, v in record.items()} for record in res]
        ids = []
        try:
            for m in media:
                os.remove(m['filename'])
                ids.append(m['id'])
        except Exception:
            pass

        if len(ids) > 0:
            ids = tuple(ids)
            cursor = self.dbcon.cursor()
            cursor.execute(f"delete from {self.db_media} where id in %s", (ids, ))
            self.dbcon.commit()
            cursor.close()


class OBD2:
    def __init__(self, port, stopbits=1, bytesize=8, parity='N', baudrate=9600):
        self.client = ModbusClient(method='rtu', port=port, stopbits=stopbits,
                                   bytesize=bytesize, parity=parity, baudrate=baudrate)
        self.client.connect()
        self.input_registers_count = 10
        self.holding_registers_count = 1
        self.unit = 1

        # Input
        self.millis = 0
        self.runtime = 0
        self.distance = 0
        self.temperature = 0
        self.vehicle_kmh = 0
        self.engine_rpm = 0
        self.obd_timestamp = 0

        # Holding
        self.sleep_delay = 0

    def communication(self):
        if not self.client.is_socket_open():
            print('reconnect..', end='')
            res = self.client.connect()
            print(res)
            return
        self.client.write_registers(address=0x00, values=[self.sleep_delay], unit=1)
        response = self.client.read_input_registers(address=0x00, count=self.input_registers_count, unit=self.unit)
        if os.getenv('DEBUG_OBD2') == 'yes':
            print(response)
        if not response.isError():
            response = [response.getRegister(i) for i in range(self.input_registers_count)]
            self.millis = response[0] | (response[1] << 16)
            self.runtime = response[2] | (response[3] << 16)
            self.distance = response[4] | (response[5] << 16)
            self.engine_rpm = response[6]
            self.temperature = (response[7] >> 8) & 0xFF
            self.vehicle_kmh = response[7] & 0xFF
            self.obd_timestamp = response[8] | (response[9] << 16)


class Video:
    def __init__(self, capture_channel=0, sim868=None):
        self.fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        self.vid = None
        self.vid_width, self.vid_height = None, None
        self.video = None
        self.frames = 0
        self.name = None
        self.timestamp = None
        self.timer = PT()
        self.capture_channel = capture_channel
        self.sim868 = sim868

    @property
    def vid_isOpened(self):
        return self.vid is not None and self.vid.isOpened()

    @property
    def video_isOpened(self):
        return self.video is not None and self.video.isOpened()

    def loop(self):
        if self.video is None and self.vid_isOpened:
            ret, frame = self.vid.read()
            self.vid_height, self.vid_width = frame.shape[:2]
            self.timestamp = datetime.now(tz=pytz.utc)
            self.name = f'media/{self.timestamp.timestamp()}.avi'
            self.video = cv2.VideoWriter(self.name, self.fourcc, float(10), (self.vid_width, self.vid_height))
            self.video.write(frame)
            self.frames += 1

        if self.timer.ton_reset(self.vid_isOpened and self.video_isOpened, 1000):
            ret, frame = self.vid.read()
            if frame is None:
                self.release()
                self.vid = None
            else:
                self.video.write(frame)
                self.frames += 1
                if self.frames == 60:
                    self.video.release()
                    self.sim868.add_media(self.name, self.timestamp)
                    self.timestamp = datetime.now(tz=pytz.utc)
                    self.name = f'media/{self.timestamp.timestamp()}.avi'
                    self.video = cv2.VideoWriter(self.name, self.fourcc, float(10), (self.vid_width, self.vid_height))
                    self.frames = 0

        if not self.vid_isOpened:
            self.release()
            self.vid = cv2.VideoCapture(self.capture_channel)

    def release(self):
        if self.video_isOpened:
            self.video.release()
            self.video = None
            self.sim868.add_media(self.name, self.timestamp)


def main():
    obd2 = OBD2(port=os.getenv('TTY_OBD2', '/dev/ttyOBD2'), baudrate=57600)
    dbcred = (dbname, username, passwd, host, port)
    sim868 = SIM868(port=os.getenv('TTY_SIM868', '/dev/ttySIM868'), baudrate=57600, dbcred=dbcred)
    gnssTon = PT()
    clearTon = PT()
    video = Video(capture_channel=int(os.getenv('VIDEOCAPTURE_NUM', 0)), sim868=sim868)

    sim868.clear_snapshots()
    sim868.clear_media()
    sim868.clear_null_session()

    datetime_correct = False

    powersave = PT()
    powersave.tof(True, 0)

    session_datetime = None
    _session_datetime = datetime.now(tz=pytz.utc)
    while True:
        time.sleep(0.1)
        obd2.communication()
        sim868.loop()
        video.loop()

        sleep_tof = int(os.getenv('SLEEP_TOF', 600000))
        if sleep_tof > 0 and not powersave.tof(obd2.runtime > 0, sleep_tof):
            sim868.powerdown()
            obd2.sleep_delay = 3
            obd2.communication()
            video.release()
            while True:
                pass

        if not datetime_correct:
            try:
                gnss = sim868.gnss.split(',')
                if len(gnss) == 21:
                    UTC_datetime = datetime.strptime(f'{gnss[2]}+0000', '%Y%m%d%H%M%S.%f%z')
                    if UTC_datetime.year > 1980:
                        td = datetime.now(tz=pytz.utc) - _session_datetime
                        datetime_correct = True
                        os.system('date -s "%s"' % UTC_datetime)
                        powersave.tof(True, 0)
                        session_datetime = UTC_datetime - td
                        sim868.update_null_session(session_datetime)
            except Exception:
                pass

        if gnssTon.ton_reset(True or obd2.runtime > 0, int(os.getenv('SNAPSHOT_TIME', 1e9))):
            snapshot = {
                'mcu_millis': obd2.millis,
                'imei': sim868.imei,
                'gnss': sim868.gnss,
                'session_datetime': session_datetime,
                'obd2_datetime': datetime.now(tz=pytz.utc) + timedelta(milliseconds=(obd2.obd_timestamp - obd2.millis)),
                'run_time': obd2.runtime,
                'distance': obd2.distance,
                'engine_rpm': obd2.engine_rpm,
                'vehicle_kmh': obd2.vehicle_kmh
            }
            sim868.gnss = ''
            snapshot_id = sim868.add_snapshot(snapshot)
            print(snapshot_id, snapshot)

        if clearTon.ton_reset(True, 10 * 60000):  # ???????????? 10 ??????????
            sim868.clear_snapshots()
            sim868.clear_media()


if __name__ == '__main__':
    main()

import json
import re
import time

import serial
from pymodbus.client.sync import ModbusSerialClient as ModbusClient

from codesys_utils import PT


class SIM868:
    def __init__(self, port, baudrate=9600, parity=serial.PARITY_NONE, stopbits=1, bytesize=8, timeout=1):
        self.port = serial.Serial(port=port, baudrate=baudrate, parity=parity,
                                  stopbits=stopbits, bytesize=bytesize, timeout=timeout)
        self.cgnsurc = 2
        self.telemetry_url = 'http://dkotlyar.ru:8000/telemetry'

        self.rx_buffer = ''
        self.tx_buffer = []
        self.loop_state = 'INIT'
        self.http_state = 'INIT'
        self.http_timeout_ton = PT()
        self.status = 'OK'
        self.lastcommand_timeout = PT()
        self.imei = ''
        self.gnss = ''
        self.snapshots = []

    def put(self, str):
        write = f"{str}\r\n".encode('utf-8')
        print(f"$ {str}")
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
            if code >= 600:
                self.http_state = 'HTTP_NETWORK_ERROR'
            else:
                self.http_state = 'HTTP_OK'  # любая ошибка от сервера считает ОК
        elif msg == 'DOWNLOAD':
            self.status = 'OK'
        else:
            if self.status == 'WAIT_IMEI':
                self.imei = msg
                self.busy()

    def communication(self):
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
            if len(self.snapshots) > 0:
                print(f'# snapshots {len(self.snapshots)}')
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
                    self.tx_buffer.append(f'AT+HTTPPARA=URL,{self.telemetry_url}')
                    self.busy()
                    self.http_state = 'HTTP_DATA'
        elif self.http_state == 'HTTP_DATA':
            if not self.is_busy():
                if self.status == 'ERROR':
                    self.http_state = 'HTTP_NETWORK_ERROR'
                else:
                    snapshot = json.dumps(self.snapshots[0])
                    self.tx_buffer.append(f'AT+HTTPDATA={len(snapshot)},2000')
                    self.busy()
                    self.http_state = 'HTTP_DOWNLOAD'
        elif self.http_state == 'HTTP_DOWNLOAD':
            if not self.is_busy():
                if self.status == 'ERROR':
                    self.http_state = 'HTTP_NETWORK_ERROR'
                else:
                    snapshot = json.dumps(self.snapshots[0])
                    self.tx_buffer.append(snapshot)
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
            self.snapshots = self.snapshots[1:]
        elif self.http_state == 'HTTP_NETWORK_ERROR':
            self.tx_buffer.append('AT+HTTPTERM')
            self.busy()
            self.http_state = 'INIT'
        else:
            print(f'Unknown http state: {self.http_state}')
            self.http_state = 'INIT'

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
            self.http_loop()
        else:
            print(f'Incorrect loop state: {self.loop_state}')
            self.loop_state = 'INIT'


class OBD2:
    def __init__(self, port, stopbits=1, bytesize=8, parity='N', baudrate=9600):
        self.client = ModbusClient(method='rtu', port=port, stopbits=stopbits,
                                   bytesize=bytesize, parity=parity, baudrate=baudrate)
        self.client.connect()
        self.input_registers_count = 10
        self.holding_registers_count = 1
        self.unit = 1

        self.millis = 0
        self.runtime = 0
        self.distance = 0
        self.temperature = 0
        self.vehicle_kmh = 0
        self.engine_rpm = 0
        self.obd_timestamp = 0

    def communication(self):
        response = self.client.read_input_registers(address=0x00, count=self.input_registers_count, unit=self.unit)
        if not response.isError():
            response = [response.getRegister(i) for i in range(self.input_registers_count)]
            self.millis = response[0] | (response[1] << 16)
            self.runtime = response[2] | (response[3] << 16)
            self.distance = response[4] | (response[5] << 16)
            self.engine_rpm = response[6]
            self.temperature = (response[7] >> 8) & 0xFF
            self.vehicle_kmh = response[7] & 0xFF
            self.obd_timestamp = response[8] | (response[9] << 16)


if __name__ == '__main__':
    obd2 = OBD2(port='/dev/ttyUSB0', baudrate=57600)
    sim868 = SIM868(port='/dev/ttyUSB1', baudrate=115200)
    gnssTon = PT()
    while True:
        time.sleep(0.1)
        obd2.communication()
        sim868.loop()
        if gnssTon.ton_reset(True, 5000):
            snapshot = {
                'ticks': obd2.millis,
                'imei': sim868.imei,
                'gps': sim868.gnss,
                'gps_timestamp': 0,
                'session': '',
                'obd2_timestamp': obd2.obd_timestamp,
                'run_time': obd2.runtime,
                'distance': obd2.distance,
                'engine_rpm': obd2.engine_rpm,
                'vehicle_kmh': obd2.vehicle_kmh
            }
            sim868.gnss = ''
            print(snapshot)
            sim868.snapshots.append(snapshot)

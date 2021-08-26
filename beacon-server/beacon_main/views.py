import base64
import json
import os
import uuid
from datetime import datetime, timedelta
import pytz

from django.core.paginator import Paginator, PageNotAnInteger, EmptyPage
from django.http import HttpResponse, JsonResponse, HttpResponseServerError
from django.views.decorators.csrf import csrf_exempt

from beacon_main.models import MediaPart, Media, Car, TelemetryRaw, Telemetry, Session
from beacon_main.utils import get_or_default, to_list, perpage, pages


@csrf_exempt
def get_test(request):
    if request.method == 'GET':
        print(request.GET.get('foo'))
        return HttpResponse(status=200)
    return HttpResponse(status=400)


@csrf_exempt
def post_test(request):
    if request.method == 'POST':
        print(request.body)
        return HttpResponse(status=200)
    return HttpResponse(status=400)


@csrf_exempt
def telemetry(request, id=None):
    if request.method == 'POST':
        body = json.loads(request.body)
        print(body)
        if type(body) == list:
            tids = [add_telemetry(t) for t in body]
            return JsonResponse({
                'error_code': 0,
                'telemetry_ids': tids
            })
        else:
            tid = add_telemetry(body)
            return JsonResponse({
                'error_code': 0,
                'telemetry_id': tid
            })

    return HttpResponse(status=400)


def add_telemetry(body):
    telemetry_raw = TelemetryRaw()
    telemetry_raw.imei = get_or_default(body, 'imei', default='')
    telemetry_raw.ticks = get_or_default(body, ('ticks', 'mcu_millis'), default=0)
    telemetry_raw.obd2_timestamp = get_or_default(body, 'obd2_timestamp', default=0)
    telemetry_raw.run_time = get_or_default(body, ('run_time', 'runtime'), default=0)
    telemetry_raw.distance = get_or_default(body, 'distance', default=0)
    telemetry_raw.engine_rpm = get_or_default(body, 'engine_rpm', default=0)
    telemetry_raw.vehicle_kmh = get_or_default(body, 'vehicle_kmh', default=0)
    telemetry_raw.gps_timestamp = get_or_default(body, 'gps_timestamp', default=0)
    telemetry_raw.gps = get_or_default(body, ('gps', 'gnss'), default='')

    duplicates = TelemetryRaw.objects.filter(imei=telemetry_raw.imei, ticks=telemetry_raw.ticks, gps=telemetry_raw.gps)
    if len(duplicates) > 0:
        telemetry_raw = duplicates[0]
    else:
        telemetry_raw.save()

    telemetry_parsed = Telemetry()
    car = Car.get_or_create(telemetry_raw.imei)
    if (session_datetime := get_or_default(body, 'session_datetime', default='')) != '':
        telemetry_parsed.session = Session.get_or_create(car=car, dt=datetime.fromtimestamp(session_datetime, tz=pytz.utc))
    if (obd2_datetime := get_or_default(body, 'obd2_datetime', default='')) != '':
        telemetry_parsed.obd2_datetime = datetime.fromtimestamp(obd2_datetime, tz=pytz.utc)
    if (snapshot_datetime := get_or_default(body, 'snapshot_datetime', default='')) != '':
        telemetry_parsed.snapshot_datetime = datetime.fromtimestamp(snapshot_datetime, tz=pytz.utc)

    duplicates = Telemetry.objects.filter(session=telemetry_parsed.session, snapshot_datetime=telemetry_parsed.snapshot_datetime)
    if len(duplicates) > 0:
        telemetry_parsed = duplicates[0]

    telemetry_parsed.run_time = telemetry_raw.run_time
    telemetry_parsed.distance = telemetry_raw.distance
    telemetry_parsed.engine_rpm = telemetry_raw.engine_rpm
    telemetry_parsed.vehicle_kmh = telemetry_raw.vehicle_kmh

    try:
        GNSS_run_status, fix_status, UTC_datetime, latitude, longitude, MLS_Altitude, \
        speed_over_ground, course_over_ground, fix_mode, _, HDOP, PDOP, VDOP, _, satellites_in_view, \
        satellites_used, _, _, cn0_max, HPA, VPA = telemetry_raw.gps.split(',')

        telemetry_parsed.gps_datetime = datetime.strptime(f'{UTC_datetime}+0000', '%Y%m%d%H%M%S.%f%z')

        if (telemetry_parsed.gps_datetime.year > 1980):
            telemetry_parsed.GNSS_run_status = GNSS_run_status == '1'
            telemetry_parsed.fix_status = fix_status == '1'
            telemetry_parsed.latitude = float(latitude) if latitude != '' else None
            telemetry_parsed.longitude = float(longitude) if longitude != '' else None
            telemetry_parsed.MLS_Altitude = float(MLS_Altitude) if MLS_Altitude != '' else None
            telemetry_parsed.speed_over_ground = float(speed_over_ground) if speed_over_ground != '' else None
            telemetry_parsed.course_over_ground = float(course_over_ground) if course_over_ground != '' else None
            telemetry_parsed.fix_mode = int(fix_mode) if fix_mode != '' else None
            telemetry_parsed.HDOP = float(HDOP) if HDOP != '' else None
            telemetry_parsed.PDOP = float(PDOP) if PDOP != '' else None
            telemetry_parsed.VDOP = float(VDOP) if VDOP != '' else None
            telemetry_parsed.satellites_in_view = int(satellites_in_view) if satellites_in_view != '' else None
            telemetry_parsed.satellites_used = int(satellites_used) if satellites_used != '' else None
            telemetry_parsed.cn0_max = int(cn0_max) if cn0_max != '' else None
            telemetry_parsed.HPA = float(HPA) if HPA != '' else None
            telemetry_parsed.VPA = float(VPA) if VPA != '' else None
    except Exception as e:
        print(e)

    telemetry_parsed.save()

    return telemetry_parsed.id


@csrf_exempt
def telemetries(request):
    try:
        cond = {}
        if (imei := request.GET.get('imei', '')) != '':
            print('imei', imei)
            cond['session__car__imei'] = imei
        if (session_id := request.GET.get('session_id', '')) != '':
            cond['session_id'] = session_id
        print(cond)

        tlmtrs = Telemetry.objects.filter(**cond)

        per_page = perpage(request.GET.get('per_page', ''))
        page = request.GET.get('page', 1)
        paginator = Paginator(tlmtrs, per_page)

        try:
            tlmtrs = paginator.get_page(page)
        except PageNotAnInteger:
            tlmtrs = paginator.get_page(1)
        except EmptyPage:
            tlmtrs = paginator.get_page(paginator.num_pages)

        return JsonResponse({
            'error_code': 0,
            'telemetries': to_list(tlmtrs.object_list),
            'pages': pages(tlmtrs)
       })
    except Exception as e:
        print(e)
        return JsonResponse({
            'error_code': 1,
            'err': e
        })


@csrf_exempt
def media_upload(request, imei, timestamp, format, parts, part):
    try:
        if request.method == 'POST':
            car = Car.get_or_create(imei)
            media = Media.objects.filter(car=car, timestamp=timestamp, format=format)
            if len(media) > 0:
                return JsonResponse({
                    'error_code': 0,
                    'media_id': media[0].id
                })

            payload = request.body.decode('utf-8')
            duplicates = MediaPart.objects.filter(car=car, timestamp=timestamp, format=format, part=part)
            media_part = MediaPart()
            if len(duplicates) == 0:
                exist_parts = MediaPart.objects.filter(car=car, timestamp=timestamp, format=format)
                if (len(exist_parts) + 1) == parts:
                    payloads = [''] * parts
                    for mp in exist_parts:
                        with open(mp.payload_filename, 'r') as f:
                            payloads[mp.part] = f.read()
                    if payloads[part] == '':
                        payloads[part] = payload
                        media_filename = f'media/{str(uuid.uuid4())}.{format}'
                        with open(media_filename, 'wb') as f:
                            f.write(base64.b64decode(''.join(payloads)))
                        for mp in exist_parts:
                            os.remove(mp.payload_filename)
                        exist_parts.delete()

                        media = Media()
                        media.car = car
                        media.timestamp = timestamp
                        media.format = format
                        media.filepath = media_filename
                        media.save()

                        return JsonResponse({
                            'error_code': 0,
                            'media_id': media.id
                        })
            else:
                media_part = duplicates[0]

            payload_filename = media_part.payload_filename or f'media/parts/{str(uuid.uuid4())}.part'
            with open(payload_filename, 'w') as f:
                f.write(payload)
            media_part.car = car
            media_part.timestamp = timestamp
            media_part.format = format
            media_part.parts = parts
            media_part.part = part
            media_part.payload_filename = payload_filename

            media_part.save()

            return JsonResponse({
                'error_code': 0,
                'mediapart_id': media_part.id
            })
    except Exception as e:
        print(e)

    return HttpResponse(status=400)


def handler404(request, *args, **argv):
    return HttpResponse(status=404)


def handler500(request, *args, **argv):
    print(args)
    return HttpResponseServerError(status=500)

import json
from datetime import datetime, timedelta
import pytz

from django.core.paginator import Paginator, PageNotAnInteger, EmptyPage
from django.http import HttpResponse, JsonResponse, HttpResponseServerError
from django.views.decorators.csrf import csrf_exempt

from beacon_main.models import Telemetry
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
        telemetry = Telemetry()
        telemetry.ticks = get_or_default(body, 'ticks', default=0)
        telemetry.imei = get_or_default(body, 'imei', default='')
        telemetry.obd2_timestamp = get_or_default(body, 'obd2_timestamp', default=0)
        telemetry.run_time = get_or_default(body, 'run_time', default=0)
        telemetry.distance = get_or_default(body, 'distance', default=0)
        telemetry.engine_rpm = get_or_default(body, 'engine_rpm', default=0)
        telemetry.vehicle_kmh = get_or_default(body, 'vehicle_kmh', default=0)
        telemetry.gps_timestamp = get_or_default(body, 'gps_timestamp', default=0)
        telemetry.gps = get_or_default(body, 'gps', default='')
        telemetry.session = get_or_default(body, 'session', default='')

        if telemetry.session != '':
            telemetry.session_datetime = datetime.strptime(f'{telemetry.session}+0000', '%Y%m%d%H%M%S.%f%z')

        try:
            GNSS_run_status, fix_status, UTC_datetime, latitude, longitude, MLS_Altitude, \
            speed_over_ground, course_over_ground, fix_mode, _, HDOP, PDOP, VDOP, _, satellites_in_view, \
            satellites_used, _, _, cn0_max, HPA, VPA = telemetry.gps.split(',')

            gps_datetime = datetime.strptime(f'{UTC_datetime}+0000', '%Y%m%d%H%M%S.%f%z')
            print(gps_datetime)

            if (gps_datetime.year > 1980):
                telemetry.snapshot_datetime = gps_datetime + timedelta(milliseconds=(telemetry.ticks - telemetry.gps_timestamp))
                telemetry.obd2_datetime = gps_datetime + timedelta(milliseconds=(telemetry.obd2_timestamp - telemetry.gps_timestamp))
                # telemetry.session_datetime = gps_datetime + timedelta(milliseconds=-telemetry.gps_timestamp)

                telemetry.GNSS_run_status = GNSS_run_status == '1'
                telemetry.fix_status = fix_status == '1'
                telemetry.gps_datetime = gps_datetime
                telemetry.latitude = float(latitude) if latitude != '' else None
                telemetry.longitude = float(longitude) if longitude != '' else None
                telemetry.MLS_Altitude = float(MLS_Altitude) if MLS_Altitude != '' else None
                telemetry.speed_over_ground = float(speed_over_ground) if speed_over_ground != '' else None
                telemetry.course_over_ground = float(course_over_ground) if course_over_ground != '' else None
                telemetry.fix_mode = int(fix_mode) if fix_mode != '' else None
                telemetry.HDOP = float(HDOP) if HDOP != '' else None
                telemetry.PDOP = float(PDOP) if PDOP != '' else None
                telemetry.VDOP = float(VDOP) if VDOP != '' else None
                telemetry.satellites_in_view = int(satellites_in_view) if satellites_in_view != '' else None
                telemetry.satellites_used = int(satellites_used) if satellites_used != '' else None
                telemetry.cn0_max = int(cn0_max) if cn0_max != '' else None
                telemetry.HPA = float(HPA) if HPA != '' else None
                telemetry.VPA = float(VPA) if VPA != '' else None
        except Exception as e:
            print(e)

        telemetry.save()

        return JsonResponse({
            'error_code': 0,
            'telemetry_id': telemetry.id
        })

    return HttpResponse(status=400)


@csrf_exempt
def telemetries(request):
    try:
        cond = {}
        if (imei := request.GET.get('imei', '')) != '':
            print('imei', imei)
            cond['imei'] = imei
        if (session := request.GET.get('session', '')) != '':
            print('session', session)
            dt = datetime.fromtimestamp(int(session)/1000)
            cond['session_datetime__gt'] = dt - timedelta(seconds=3)
            cond['session_datetime__lt'] = dt + timedelta(seconds=3)
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


def repair_data(request):
    telemetries = Telemetry.objects.all()
    for t in telemetries:
        try:
            GNSS_run_status, fix_status, UTC_datetime, latitude, longitude, MLS_Altitude, \
            speed_over_ground, course_over_ground, fix_mode, _, HDOP, PDOP, VDOP, _, satellites_in_view, \
            satellites_used, _, _, cn0_max, HPA, VPA = t.gps.split(',')

            gps_datetime = datetime.strptime(UTC_datetime, '%Y%m%d%H%M%S.%f')

            if (gps_datetime.year > 1980):
                t.snapshot_datetime = gps_datetime + timedelta(milliseconds=(t.ticks - t.gps_timestamp))
                t.obd2_datetime = gps_datetime + timedelta(milliseconds=(t.obd2_timestamp - t.gps_timestamp))
                t.session_datetime = gps_datetime + timedelta(milliseconds=-t.gps_timestamp)

                t.GNSS_run_status = GNSS_run_status == '1'
                t.fix_status = fix_status == '1'
                t.gps_datetime = gps_datetime
                t.latitude = float(latitude) if latitude != '' else None
                t.longitude = float(longitude) if longitude != '' else None
                t.MLS_Altitude = float(MLS_Altitude) if MLS_Altitude != '' else None
                t.speed_over_ground = float(speed_over_ground) if speed_over_ground != '' else None
                t.course_over_ground = float(course_over_ground) if course_over_ground != '' else None
                t.fix_mode = int(fix_mode) if fix_mode != '' else None
                t.HDOP = float(HDOP) if HDOP != '' else None
                t.PDOP = float(PDOP) if PDOP != '' else None
                t.VDOP = float(VDOP) if VDOP != '' else None
                t.satellites_in_view = int(satellites_in_view) if satellites_in_view != '' else None
                t.satellites_used = int(satellites_used) if satellites_used != '' else None
                t.cn0_max = int(cn0_max) if cn0_max != '' else None
                t.HPA = float(HPA) if HPA != '' else None
                t.VPA = float(VPA) if VPA != '' else None
        except Exception:
            pass

        t.save()
    return HttpResponse()


def handler404(request, *args, **argv):
    return HttpResponse(status=404)


def handler500(request, *args, **argv):
    print(args)
    return HttpResponseServerError(status=500)

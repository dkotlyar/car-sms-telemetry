import json
import os
from datetime import datetime, timedelta

import pytz
from django.db.models import F
from django.http import HttpResponse
from django.views.decorators.csrf import csrf_exempt

from beacon_local.models import Snapshot, Media
from beacon_local.utils import default_json_pagination, default_json_error, default_json_response, object_to_bool


def get_test(request):
    print(request)
    return HttpResponse(status=200)


@csrf_exempt
def post_test(request):
    print(request.body)
    return default_json_response()


def check_new_records(request):
    snapshots = Snapshot.unpublished()
    media = Media.unpublished()
    return default_json_response({
        'snapshots': len(snapshots),
        'media': len(media)
    })


def get_unpublished_snapshots(request):
    snapshots = Snapshot.objects.filter(published=False)
    return default_json_pagination(request, snapshots)


@csrf_exempt
def set_snapshots_statuses(request):
    if request.method == 'POST':
        body = json.loads(request.body)
        if 'snapshots' in body:
            body = body['snapshots']

        for snapshot in body:
            snapshot_datetime = datetime.fromtimestamp(snapshot['snapshot_datetime'] / 1000, pytz.utc)
            Snapshot.objects\
                .filter(imei__exact=snapshot['imei'],
                        snapshot_datetime__gte=snapshot_datetime - timedelta(milliseconds=10),
                        snapshot_datetime__lte=snapshot_datetime + timedelta(milliseconds=10))\
                .update(published=snapshot['published'])

        return default_json_response({})

    return HttpResponse(status=400)


def get_unpublished_media(request):
    medias = Media.unpublished()
    return default_json_pagination(request, medias)


def get_media_file(request, id):
    try:
        media = Media.objects.filter(id=id)
        if len(media) > 0:
            media = media[0]
            with open(media.filename, 'rb') as f:
                file_data = f.read()

            response = HttpResponse(file_data, content_type='application/octet-stream')
            _, filename = os.path.split(media.filename)
            response['Content-Disposition'] = f'attachment; filename="{filename}"'
            return response

        return HttpResponse(status=404)
    except Exception as e:
        return HttpResponse(status=500)

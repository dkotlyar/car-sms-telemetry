import json
import os

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


def get_unpublished_snapshots(request):
    snapshots = Snapshot.objects.filter(published=False)
    return default_json_pagination(request, snapshots)


@csrf_exempt
def set_snapshots_statuses(request):
    if request.method == 'POST':
        body = json.loads(request.body)
        if not ('ids' in body):
            return default_json_error('Field `ids` not in request')
        if not ('published' in body):
            return default_json_error('Field `published` not in request')

        snapshots = Snapshot.objects.filter(id__in=list(map(int, body['ids'])))
        published = object_to_bool(body['published'])
        ids = [s.id for s in snapshots]
        snapshots.update(published=published)

        return default_json_response({
            'ids': ids,
            'published': published
        })

    return HttpResponse(status=400)


def get_unpublished_media(request):
    medias = Media.objects.filter(published__lt=F('parts'))
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

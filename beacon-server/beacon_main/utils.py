from django.core.paginator import Paginator, PageNotAnInteger, EmptyPage
from django.db import models
from django.forms import model_to_dict
from django.http import JsonResponse


def get_or_default(obj, field, default=None):
    if type(field) == list or type(field) == tuple:
        for f in field:
            if f in obj:
                return obj[f]
    return obj[field] if field in obj else default

def to_dict(instance, ignore_to_dict=False, exclude=None):
    if instance is None:
        return None

    if hasattr(instance, 'to_dict') and not ignore_to_dict:
        return instance.to_dict()

    dict = model_to_dict(instance, exclude=exclude)
    for f in instance._meta.fields:
        if type(f) == models.ForeignKey and not (exclude and '%s_id' % f.name in exclude):
            dict['%s_id' % f.name] = getattr(instance, '%s_id' % f.name)

        if exclude and f.name in exclude:
            continue

        obj = getattr(instance, f.name)
        if obj is not None:
            if type(f) == models.ForeignKey:
                dict[f.name] = to_dict(obj)
            if type(f) == models.DateTimeField:
                dict[f.name] = int(obj.timestamp()*1000)
        else:
            dict[f.name] = None
    return dict


def to_list(object_list, exclude=None):
    return [to_dict(instance, exclude=exclude) for instance in object_list]


def perpage(per_page, default=20):
    if per_page == '0':
        return 1e9
    elif hasattr(per_page, 'isdigit') and per_page.isdigit():
        return int(per_page)
    else:
        return default


def pages(paginator):
    page_range = [p for p in paginator.paginator.page_range if paginator.number - 5 <= p <= paginator.number + 5]
    return {
        'has_other_pages': paginator.has_other_pages(),
        'next_page_number': paginator.next_page_number() if paginator.has_next() else None,
        'previous_page_number': paginator.previous_page_number() if paginator.has_previous() else None,
        'page_range': page_range,
        'number': paginator.number
    }


def default_json_pagination(request, items):
    per_page = perpage(request.GET.get('per_page', ''))
    page = request.GET.get('page', 1)
    paginator = Paginator(items, per_page)

    try:
        items = paginator.get_page(page)
    except PageNotAnInteger:
        items = paginator.get_page(1)
    except EmptyPage:
        items = paginator.get_page(paginator.num_pages)

    return JsonResponse({
        'error_code': 0,
        'items': to_list(items.object_list),
        'pages': pages(items)
    })
from django.http import HttpResponse
from django.shortcuts import render

# Create your views here.


def get_test(request):
    print(request)
    return HttpResponse(status=200)


def post_test(request):
    print(request.body)
    return HttpResponse(status=200)
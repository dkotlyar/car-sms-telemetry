from django.urls import path
from . import views

urlpatterns = [
    path('telemetry', views.telemetry),
    path('telemetries', views.telemetries),
    path('sessions', views.sessions),
    path('media/<imei>/<int:timestamp>/<format>/<int:parts>/<int:part>', views.media_upload),
    path('media/<int:id>', views.get_media_file),
    path('get_test', views.get_test),
    path('post_test', views.post_test)
]

from django.urls import path
from . import views

urlpatterns = [
    path('telemetry', views.telemetry),
    path('telemetries', views.telemetries),
    path('media/<imei>/<int:timestamp>/<format>/<int:parts>/<int:part>', views.media_upload),
    path('get_test', views.get_test),
    path('post_test', views.post_test),
    path('repair_data', views.repair_data)
]

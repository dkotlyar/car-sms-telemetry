from django.db import models


class Snapshot(models.Model):
    imei = models.CharField(max_length=20, blank=True)
    gnss = models.CharField(max_length=200, blank=True)
    runtime = models.IntegerField(default=0)
    distance = models.IntegerField(default=0)
    vehicle_kmh = models.IntegerField(default=0)
    engine_rpm = models.IntegerField(default=0)
    snapshot_datetime = models.DateTimeField(null=True)
    session_datetime = models.DateTimeField(null=True)
    obd2_datetime = models.DateTimeField(null=True)
    mcu_millis = models.IntegerField(default=0)
    published = models.BooleanField(default=False)


class Media(models.Model):
    filename = models.CharField(max_length=100)
    create_datetime = models.DateTimeField(auto_now_add=True)
    published = models.IntegerField(default=0)
    parts = models.IntegerField(default=0)
    timestamp = models.DateTimeField(null=True)

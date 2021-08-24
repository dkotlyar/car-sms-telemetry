from django.db import models

# Create your models here.


class Telemetry(models.Model):
    create_datetime = models.DateTimeField(auto_now_add=True)
    update_datetime = models.DateTimeField(auto_now=True)
    imei = models.CharField(max_length=20, blank=True)
    session = models.CharField(max_length=20, blank=True)
    session_datetime = models.DateTimeField(null=True)
    snapshot_datetime = models.DateTimeField(null=True)
    ticks = models.IntegerField(default=0)
    obd2_datetime = models.DateTimeField(null=True)
    obd2_timestamp = models.IntegerField(default=0)
    run_time = models.IntegerField(default=0)
    distance = models.IntegerField(default=0)
    engine_rpm = models.IntegerField(default=0)
    vehicle_kmh = models.IntegerField(default=0)
    gps_timestamp = models.IntegerField(default=0)
    gps = models.CharField(max_length=200)
    GNSS_run_status = models.BooleanField(default=False)
    fix_status = models.BooleanField(default=False)
    gps_datetime = models.DateTimeField(null=True)
    latitude = models.FloatField(null=True)
    longitude = models.FloatField(null=True)
    MLS_Altitude = models.FloatField(null=True)
    speed_over_ground = models.FloatField(null=True)
    course_over_ground = models.FloatField(null=True)
    fix_mode = models.IntegerField(null=True)
    HDOP = models.FloatField(null=True)
    PDOP = models.FloatField(null=True)
    VDOP = models.FloatField(null=True)
    satellites_in_view = models.IntegerField(null=True)
    satellites_used = models.IntegerField(null=True)
    cn0_max = models.IntegerField(null=True)
    HPA = models.FloatField(null=True)
    VPA = models.FloatField(null=True)

    class Meta:
        ordering = ['-gps_datetime']

    def __str__(self):
        return 'Telemetry #%d %s :: %f %f' % (self.id, self.imei, self.latitude or 0, self.longitude or 0)


class MediaPart(models.Model):
    create_datetime = models.DateTimeField(auto_now_add=True)
    update_datetime = models.DateTimeField(auto_now=True)
    imei = models.CharField(max_length=20, blank=True)
    timestamp = models.BigIntegerField()
    format = models.CharField(max_length=10, blank=True)
    payload_filename = models.CharField(max_length=100)
    part = models.IntegerField()
    parts = models.IntegerField()


class Media(models.Model):
    create_datetime = models.DateTimeField(auto_now_add=True)
    update_datetime = models.DateTimeField(auto_now=True)
    imei = models.CharField(max_length=20, blank=True)
    timestamp = models.BigIntegerField()
    format = models.CharField(max_length=10, blank=True)
    filepath = models.CharField(max_length=100)

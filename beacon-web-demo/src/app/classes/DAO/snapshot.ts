export class Snapshot {
  id: number = 0;
  createDatetime: Date|undefined;
  updateDatetime: Date|undefined;
  imei: string = '';
  sessionDatetime: Date|undefined;
  snapshotDatetime: Date|undefined;
  obd2Datetime: Date|undefined;
  runTime: number = 0;
  distance: number = 0;
  engineRpm: number = 0;
  vehicleKmh: number = 0;
  gpsDatetime: Date|undefined;
  latitude: number = 0;
  longitude: number = 0;
  satellitesInView: number = 0;
  satellitesUsed: number = 0;

  static from_django(item: any): Snapshot|undefined {
    if (item === undefined || item === null) {
      return undefined;
    }
    const snapshot = new Snapshot();
    snapshot.id = item.id;
    snapshot.createDatetime = new Date(item.create_datetime);
    snapshot.updateDatetime = new Date(item.update_datetime);
    snapshot.sessionDatetime = new Date(item.session_datetime);
    snapshot.snapshotDatetime = new Date(item.snapshot_datetime);
    snapshot.obd2Datetime = new Date(item.obd2_datetime);
    snapshot.gpsDatetime = new Date(item.gps_datetime);
    snapshot.imei = item.imei;
    snapshot.runTime = item.run_time;
    snapshot.distance = item.distance;
    snapshot.engineRpm = item.engine_rpm;
    snapshot.vehicleKmh = item.vehicle_kmh;
    snapshot.latitude = item.latitude;
    snapshot.longitude = item.longitude;
    snapshot.satellitesInView = item.satellites_in_view;
    snapshot.satellitesUsed = item.satellites_used;
    return snapshot;
  }
}

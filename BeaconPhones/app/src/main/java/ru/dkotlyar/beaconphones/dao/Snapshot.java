package ru.dkotlyar.beaconphones.dao;

import java.util.Date;

public class Snapshot {
    private Integer id;
    private String imei;
    private String gnss;
    private Integer runtime;
    private Integer distance;
    private Integer vehicleKmh;
    private Integer engineRpm;
    private Date snapshotDatetime;
    private Date sessionDatetime;
    private Date obd2Datetime;
    private Integer mcuMillis;
    private Boolean published;
    private Integer localId;
    private Boolean syncedBack;

    public Integer getId() {
        return id;
    }

    public void setId(Integer id) {
        this.id = id;
    }

    public String getImei() {
        return imei;
    }

    public void setImei(String imei) {
        this.imei = imei;
    }

    public String getGnss() {
        return gnss;
    }

    public void setGnss(String gnss) {
        this.gnss = gnss;
    }

    public Integer getRuntime() {
        return runtime;
    }

    public void setRuntime(Integer runtime) {
        this.runtime = runtime;
    }

    public Integer getDistance() {
        return distance;
    }

    public void setDistance(Integer distance) {
        this.distance = distance;
    }

    public Integer getVehicleKmh() {
        return vehicleKmh;
    }

    public void setVehicleKmh(Integer vehicleKmh) {
        this.vehicleKmh = vehicleKmh;
    }

    public Integer getEngineRpm() {
        return engineRpm;
    }

    public void setEngineRpm(Integer engineRpm) {
        this.engineRpm = engineRpm;
    }

    public Date getSnapshotDatetime() {
        return snapshotDatetime;
    }

    public void setSnapshotDatetime(Date snapshotDatetime) {
        this.snapshotDatetime = snapshotDatetime;
    }

    public Date getSessionDatetime() {
        return sessionDatetime;
    }

    public void setSessionDatetime(Date sessionDatetime) {
        this.sessionDatetime = sessionDatetime;
    }

    public Date getObd2Datetime() {
        return obd2Datetime;
    }

    public void setObd2Datetime(Date obd2Datetime) {
        this.obd2Datetime = obd2Datetime;
    }

    public Integer getMcuMillis() {
        return mcuMillis;
    }

    public void setMcuMillis(Integer mcuMillis) {
        this.mcuMillis = mcuMillis;
    }

    public Boolean getPublished() {
        return published;
    }

    public void setPublished(Boolean published) {
        this.published = published;
    }

    public Integer getLocalId() {
        return localId;
    }

    public void setLocalId(Integer localId) {
        this.localId = localId;
    }

    public Boolean getSyncedBack() {
        return syncedBack;
    }

    public void setSyncedBack(Boolean syncedBack) {
        this.syncedBack = syncedBack;
    }
}

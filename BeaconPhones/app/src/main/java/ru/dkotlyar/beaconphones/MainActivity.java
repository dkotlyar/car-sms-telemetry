package ru.dkotlyar.beaconphones;

import androidx.appcompat.app.AppCompatActivity;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.toolbox.JsonObjectRequest;
import com.android.volley.toolbox.Volley;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.stream.Collectors;

import ru.dkotlyar.beaconphones.dao.Snapshot;

public class MainActivity extends AppCompatActivity {

    private final String SERVER_URL = "http://dkotlyar.ru:8000";
//    private final String SERVER_URL = "http://192.168.0.199:8000";

    private RecordsDB recordsDB;
    private RequestQueue queue;
    private int downloadedSnapshots = 0;
    private int uploadedSnapshots = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        this.recordsDB = new RecordsDB(getBaseContext());
        queue = Volley.newRequestQueue(this);
        this.setDownloadedSnapshots(this.recordsDB.selectSnapshots().size());
        this.setUploadedSnapshots(this.recordsDB.selectSnapshots(new HashMap<String, String>(){{
            put("published", "1");
        }}).size());
    }

    @SuppressLint("SetTextI18n")
    private void setDownloadedSnapshots(int value) {
        this.downloadedSnapshots = value;
        ((TextView)findViewById(R.id.downloadedSnapshots)).setText(Integer.toString(this.downloadedSnapshots));
    }

    @SuppressLint("SetTextI18n")
    private void setUploadedSnapshots(int value) {
        this.uploadedSnapshots = value;
        ((TextView)findViewById(R.id.uploadedSnapshots)).setText(Integer.toString(this.uploadedSnapshots));
    }

    private String getDeviceBaseUrl() {
        String ip = ((EditText)findViewById(R.id.ip)).getText().toString();
        return "http://" + ip + ":8000";
    }

    public void checkNewRecords(View view) {
        this.syncBack(this::checkNewRecords);
    }

    @SuppressLint("SetTextI18n")
    private void checkNewRecords() {
        JsonObjectRequest jsonObjectRequest = new JsonObjectRequest(Request.Method.GET,
                getDeviceBaseUrl() + "/check_new_records", null,
                response -> {
                    try {
                        if (response.getInt("error_code") == 0) {
                            int unpublishedSnapshots = response.getInt("snapshots");
                            int unpublishedMedia = response.getInt("media");
                            ((TextView)findViewById(R.id.unpublishedSnapshots)).setText(Integer.toString(unpublishedSnapshots));
                            ((TextView)findViewById(R.id.unpublishedMedia)).setText(Integer.toString(unpublishedMedia));
                        }
                    } catch (JSONException e) {
                        Log.e("check new records", e.toString());
                    }
                }, error -> Log.e("check new records", error.toString()));
        this.queue.add(jsonObjectRequest);
    }

    public void downloadRecords(View view) {
        this.downloadRecords(100, 1);
    }

    private void downloadRecords(Integer perpage, Integer page) {
        JsonObjectRequest jsonObjectRequest = new JsonObjectRequest
                (Request.Method.GET, getDeviceBaseUrl() +
                        "/get_unpublished_snapshots?per_page=" +
                        perpage +
                        "&page=" +
                        page, null,
                        response -> {
                            try {
                                if (response.getInt("error_code") == 0) {
                                    List<Snapshot> snapshots = new ArrayList<>();
                                    JSONArray items = response.getJSONArray("items");
                                    for (int i = 0; i < items.length(); i++) {
                                        JSONObject object = items.getJSONObject(i);
                                        Snapshot snapshot = new Snapshot();
                                        snapshot.setImei(object.getString("imei"));
                                        snapshot.setGnss(object.getString("gnss"));
                                        snapshot.setRuntime(object.getInt("runtime"));
                                        snapshot.setDistance(object.getInt("distance"));
                                        snapshot.setVehicleKmh(object.getInt("vehicle_kmh"));
                                        snapshot.setEngineRpm(object.getInt("engine_rpm"));
                                        snapshot.setSnapshotDatetime(new Date(object.getLong("snapshot_datetime")));
                                        snapshot.setSessionDatetime(new Date(object.getLong("session_datetime")));
                                        snapshot.setObd2Datetime(new Date(object.getLong("obd2_datetime")));
                                        snapshot.setMcuMillis(object.getInt("mcu_millis"));
                                        snapshot.setPublished(object.getBoolean("published"));
                                        snapshot.setLocalId(object.getInt("id"));
                                        snapshot.setSyncedBack(false);

                                        if (!this.recordsDB.hasSnapshot(snapshot)) {
                                            snapshots.add(snapshot);
                                        }
                                    }
                                    this.recordsDB.insertSnapshots(snapshots);

                                    this.setDownloadedSnapshots(this.downloadedSnapshots + snapshots.size());

                                    if (!response.getJSONObject("pages").getString("next_page_number").equals("null")) {
                                        this.downloadRecords(perpage, page + 1);
                                    }
                                }
                            } catch (JSONException e) {
                                Log.e("error", e.toString());
                            }
                        },
                        error -> Log.d("error", error.toString()));
        this.queue.add(jsonObjectRequest);
    }

    public void uploadToServer(View view) {
        JSONArray body = new JSONArray();
        List<Snapshot> snapshots = recordsDB.selectUnpublishedSnapshots();
        for (Snapshot snapshot: snapshots) {
            try {
                JSONObject object = new JSONObject();
                object.put("imei", snapshot.getImei());
                object.put("gnss", snapshot.getGnss());
                object.put("distance", snapshot.getDistance());
                object.put("runtime", snapshot.getRuntime());
                object.put("vehicle_kmh", snapshot.getVehicleKmh());
                object.put("engine_rpm", snapshot.getEngineRpm());
                object.put("snapshot_datetime", snapshot.getSnapshotDatetime().getTime());
                object.put("session_datetime", snapshot.getSessionDatetime().getTime());
                object.put("obd2_datetime", snapshot.getObd2Datetime().getTime());
                object.put("mcu_millis", snapshot.getMcuMillis());
                body.put(object);
            } catch (JSONException e) {
                Log.e("error", e.toString());
            }
        }
        if (body.length() > 0) {
            try {
                JSONObject root = new JSONObject();
                root.put("snapshots", body);
                JsonObjectRequest jsonObjectRequest = new JsonObjectRequest(
                        Request.Method.POST, this.SERVER_URL + "/telemetry", root, response -> {
                            Log.d("upload response", response.toString());
                            try {
                                if (response.getInt("error_code") == 0) {
                                    this.recordsDB.updateSnapshots(snapshots, new HashMap<String, String>() {{
                                        put("published", "1");
                                    }});
                                    this.setUploadedSnapshots(this.uploadedSnapshots + snapshots.size());
                                    this.syncBack(this::checkNewRecords);
                                }
                            } catch (JSONException e) {
                                e.printStackTrace();
                            }
                }, error -> Log.d("error", error.toString()));
                this.queue.add(jsonObjectRequest);
            } catch (JSONException e) {
                Log.e("error", e.toString());
            }
        }
    }

    private void syncBack(Runnable callback) {
        List<Snapshot> snapshots = recordsDB.selectSnapshots(new HashMap<String, String>(){{
            put("published", "1");
//            put("synced_back", "0");
        }});
        // TODO: почему-то synced_back=0 не работает :(
        snapshots = snapshots.stream().filter(snapshot -> !snapshot.getSyncedBack()).collect(Collectors.toList());
        JSONArray body = new JSONArray();
        for (Snapshot snapshot: snapshots) {
            try {
                JSONObject object = new JSONObject();
                object.put("imei", snapshot.getImei());
                object.put("snapshot_datetime", snapshot.getSnapshotDatetime().getTime());
                object.put("published", snapshot.getPublished());
                body.put(object);
            } catch (JSONException e) {
                Log.e("error", e.toString());
            }
        }

        if (body.length() > 0) {
            try {
                JSONObject root = new JSONObject();
                root.put("snapshots", body);
                List<Snapshot> finalSnapshots = snapshots;
                JsonObjectRequest jsonObjectRequest = new JsonObjectRequest(
                        Request.Method.POST, this.getDeviceBaseUrl() + "/set_snapshots_statuses", root, response -> {
                    Log.d("upload response", response.toString());
                    try {
                        if (response.getInt("error_code") == 0) {
                            this.recordsDB.updateSnapshots(finalSnapshots, new HashMap<String, String>() {{
                                put("synced_back", "1");
                            }});
                        }
                        callback.run();
                    } catch (JSONException e) {
                        e.printStackTrace();
                    }
                }, error -> Log.d("error", error.toString()));
                this.queue.add(jsonObjectRequest);
            } catch (JSONException e) {
                Log.e("error", e.toString());
            }
        } else {
            callback.run();
        }
    }
}
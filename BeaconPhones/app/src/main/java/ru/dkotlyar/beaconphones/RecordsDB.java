package ru.dkotlyar.beaconphones;

import static android.content.Context.MODE_PRIVATE;

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import ru.dkotlyar.beaconphones.dao.Snapshot;

public class RecordsDB extends SQLiteOpenHelper {

    private static final String DB_NAME = "app.db";
    public static final Integer SCHEMA = 3;

    public RecordsDB(Context context) {
        super(context, DB_NAME, null, SCHEMA);
    }


    @Override
    public void onCreate(SQLiteDatabase sqLiteDatabase) {
        sqLiteDatabase.execSQL("CREATE TABLE snapshots (" +
                "id INTEGER PRIMARY KEY AUTOINCREMENT," +
                "imei TEXT," +
                "gnss TEXT," +
                "runtime INTEGER," +
                "distance INTEGER," +
                "vehicle_kmh INTEGER," +
                "engine_rpm INTEGER," +
                "snapshot_datetime INTEGER," +
                "session_datetime INTEGER," +
                "obd2_datetime INTEGER," +
                "mcu_millis INTEGER," +
                "published INTEGER," +
                "local_id INTEGER," +
                "synced_back INTEGER);");
    }

    @Override
    public void onUpgrade(SQLiteDatabase sqLiteDatabase, int oldVersion, int newVersion) {
        if (oldVersion == 1 && newVersion > 1) {
            sqLiteDatabase.execSQL("ALTER TABLE snapshots ADD " +
                    "local_id INTEGER;");
            oldVersion = 2;
        }
        if (oldVersion == 2 && newVersion > 2) {
            sqLiteDatabase.execSQL("ALTER TABLE snapshots ADD " +
                    "synced_back INTEGER;");
            oldVersion = 3;
        }
    }

    private Cursor rawQuery(String sql, String[] selectionArgs) {
        return this.getReadableDatabase().rawQuery(sql, selectionArgs);
    }

    public void insertSnapshots(List<Snapshot> snapshots) {
        if (snapshots.size() == 0) {
            return;
        }

        StringBuilder sb = new StringBuilder();
        sb.append("INSERT INTO snapshots (imei, gnss, runtime, distance, vehicle_kmh, engine_rpm, " +
                "snapshot_datetime, session_datetime, obd2_datetime, mcu_millis, published, " +
                "local_id, synced_back) VALUES ");
        boolean comma = false;
        for (Snapshot snapshot : snapshots) {
            if (comma) {
                sb.append(", ");
            }
            sb.append("(")
                    .append("'").append(snapshot.getImei()).append("'")
                    .append(",")
                    .append("'").append(snapshot.getGnss()).append("'")
                    .append(",")
                    .append(snapshot.getRuntime())
                    .append(",")
                    .append(snapshot.getDistance())
                    .append(",")
                    .append(snapshot.getVehicleKmh())
                    .append(",")
                    .append(snapshot.getEngineRpm())
                    .append(",")
                    .append(snapshot.getSnapshotDatetime().getTime())
                    .append(",")
                    .append(snapshot.getSessionDatetime().getTime())
                    .append(",")
                    .append(snapshot.getObd2Datetime().getTime())
                    .append(",")
                    .append(snapshot.getMcuMillis())
                    .append(",")
                    .append(snapshot.getPublished() ? 1 : 0)
                    .append(",")
                    .append(snapshot.getLocalId())
                    .append(",")
                    .append(snapshot.getSyncedBack() ? 1 : 0)
                    .append(")");
            comma = true;
        }
        sb.append(";");
        getReadableDatabase().execSQL(sb.toString());
    }

    public List<Snapshot> selectSnapshots(Map<String, String> filter) {
        StringBuilder whereCond = new StringBuilder();
        String[] args = null;
        if (filter != null && filter.size() > 0) {
            whereCond.append(" WHERE ");
            args = new String[filter.size()];
            int i = 0;
            for (String key : filter.keySet()) {
                if (i > 0) {
                    whereCond.append(" AND ");
                }
                whereCond.append(key)
                        .append("=?");
                args[i++] = filter.get(key);
            }
        }

        List<Snapshot> snapshots = new ArrayList<>();
        Cursor c = this.rawQuery("SELECT id, imei, gnss, runtime, distance, vehicle_kmh, " +
                "engine_rpm, snapshot_datetime, session_datetime, obd2_datetime, mcu_millis, " +
                "published, local_id, synced_back FROM snapshots " + whereCond.toString(), args);
        c.moveToFirst();
        while (!c.isAfterLast()) {
            Snapshot snapshot = new Snapshot();
            snapshot.setId(c.getInt(0));
            snapshot.setImei(c.getString(1));
            snapshot.setGnss(c.getString(2));
            snapshot.setRuntime(c.getInt(3));
            snapshot.setDistance(c.getInt(4));
            snapshot.setVehicleKmh(c.getInt(5));
            snapshot.setEngineRpm(c.getInt(6));
            snapshot.setSnapshotDatetime(new Date(c.getLong(7)));
            snapshot.setSessionDatetime(new Date(c.getLong(8)));
            snapshot.setObd2Datetime(new Date(c.getLong(9)));
            snapshot.setMcuMillis(c.getInt(10));
            snapshot.setPublished(c.getInt(11) == 1);
            snapshot.setLocalId(c.getInt(12));
            int sb = c.getInt(13);
            snapshot.setSyncedBack(c.getInt(13) == 1);
            snapshots.add(snapshot);
            c.moveToNext();
        }
        c.close();
        return snapshots;
    }

    public List<Snapshot> selectSnapshots() {
        return this.selectSnapshots(null);
    }

    public List<Snapshot> selectUnpublishedSnapshots() {
        return this.selectSnapshots(new HashMap<String, String>() {{
            put("published", "0");
        }});
    }

    public List<Snapshot> selectSyncedBackSnapshots() {
        return this.selectSnapshots(new HashMap<String, String>() {{
            put("synced_back", "1");
        }});
    }

    public boolean hasSnapshot(Snapshot snapshot) {
        Cursor c = this.rawQuery("SELECT count(*) FROM snapshots WHERE " +
                "imei=? AND snapshot_datetime=? AND local_id=?",
                new String[] { snapshot.getImei(),
                        String.valueOf(snapshot.getSnapshotDatetime().getTime()),
                        snapshot.getLocalId().toString()
                });
        c.moveToFirst();
        return c.getInt(0) > 0;
    }

    public void updateSnapshots(List<Snapshot> snapshots, Map<String, String> updates) {
        if (snapshots.size() == 0 || updates.size() == 0) {
            return;
        }

        StringBuilder whereCond = new StringBuilder();
        StringBuilder updateExpr = new StringBuilder();
        String[] args = new String[snapshots.size() + updates.size()];

        int i = 0, j = 0;
        for (String key : updates.keySet()) {
            if (i > 0) {
                updateExpr.append(", ");
            }
            updateExpr.append(key)
                    .append("=?");
            args[i++] = updates.get(key);
        }

        whereCond.append(" WHERE id in (");
        for (Snapshot snapshot : snapshots) {
            if (j > 0) {
                whereCond.append(", ");
            }
            whereCond.append("?");
            args[i++] = snapshot.getId().toString();
            j++;
        }
        whereCond.append(")");

        this.getReadableDatabase().execSQL("UPDATE snapshots SET " + updateExpr.toString() +
                whereCond.toString(), args);
    }
}

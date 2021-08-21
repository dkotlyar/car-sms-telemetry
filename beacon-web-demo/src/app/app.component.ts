import {Component, OnInit} from '@angular/core';
import Map from 'ol/Map';
import View from 'ol/View';
import TileLayer from 'ol/layer/Tile';
import OSM from 'ol/source/OSM';
import {fromLonLat} from "ol/proj";
import Feature from "ol/Feature";
import VectorSource from "ol/source/Vector";
import VectorLayer from "ol/layer/Vector";
import {LineString, Point} from "ol/geom";
import {Fill, Icon, Stroke, Style, Circle} from "ol/style";
import {DjangoBackendService} from "./services/django-backend.service";
import {Snapshot} from "./classes/DAO/snapshot";

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css']
})
export class AppComponent implements OnInit {
  map: any;
  features: any;
  lines: any;

  constructor(private backend: DjangoBackendService) {
  }

  ngOnInit(): void {

    this.features = new VectorSource({
      features: [],
    });
    this.lines = new VectorSource({
      features: []
    });

    this.map = new Map({
      target: 'map',
      layers: [
        new TileLayer({
          source: new OSM()
        }),
        new VectorLayer({
          source: this.features,
          style: new Style({
            image: new Circle({
              radius: 9,
              fill: new Fill({color: 'red'})
            })
          })
        }),
        new VectorLayer({
          source: this.lines,
          style: new Style({
            fill: new Fill({color: 'blue'}),
            stroke: new Stroke({color: 'green', width: 2})
          })
        })
      ],
      view: new View({
        center: fromLonLat([39.012828, 58.007035]),
        zoom: 12
      })
    });

    this.loadData();
  }

  loadData(): void {
    this.backend.snapshots(1, 1000, {imei: '867190030294244', session: 1629566395000}).subscribe(res => {
      var features = [];
      var lines = [];
      var lastSnapshot: Snapshot|undefined;
      for (const snapshot of res.snapshots) {
        var f = new Feature({
          geometry: new Point(fromLonLat([snapshot.longitude, snapshot.latitude])),
          snapshot
        })
        features.push(f);
        if (lastSnapshot !== undefined) {
          var line = new Feature({
            geometry: new LineString([fromLonLat([lastSnapshot.longitude, lastSnapshot.latitude]),
              fromLonLat([snapshot.longitude, snapshot.latitude])])
          });
          lines.push(line);
        }
        lastSnapshot = snapshot;
      }
      console.log(res.snapshots);
      this.features.addFeatures(features);
      this.lines.addFeatures(lines);
    }, error => {
      console.error(error);
    });

  }
}

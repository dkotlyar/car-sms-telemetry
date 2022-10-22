import { Component, OnInit } from '@angular/core';
import {DjangoBackendService} from "../../services/django-backend.service";
import {ActivatedRoute} from "@angular/router";
import VectorSource from "ol/source/Vector";
import Map from "ol/Map";
import TileLayer from "ol/layer/Tile";
import OSM from "ol/source/OSM";
import VectorLayer from "ol/layer/Vector";
import {Circle, Fill, Stroke, Style} from "ol/style";
import View from "ol/View";
import {fromLonLat} from "ol/proj";
import {Snapshot} from "../../classes/DAO/snapshot";
import Feature from "ol/Feature";
import {LineString, Point} from "ol/geom";

@Component({
  selector: 'app-session-map',
  templateUrl: './session-map.component.html',
  styleUrls: ['./session-map.component.css']
})
export class SessionMapComponent implements OnInit {
  map: any;
  features: any;
  lines: any;
  snapshots: Snapshot[]|undefined;

  constructor(private backend: DjangoBackendService,
              private activatedRoute: ActivatedRoute) {
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
    setInterval(() => {this.loadData()}, 10000);
  }

  loadData(): void {
    this.features.features = [];
    this.lines.features = [];

    this.backend.snapshots(1, 2000, {sessionId: this.activatedRoute.snapshot.params.id}).subscribe(res => {
      var features = [];
      var lines = [];
      var lastSnapshot: Snapshot|undefined;
      this.snapshots = res.snapshots;
      for (const snapshot of res.snapshots) {
        if (snapshot.longitude !== null && snapshot.latitude !== null) {
          var f = new Feature({
            geometry: new Point(fromLonLat([snapshot.longitude, snapshot.latitude])),
            snapshot
          })
          features.push(f);
          if (lastSnapshot !== undefined) {
            var line = new Feature({
              geometry: new LineString([fromLonLat([lastSnapshot.longitude, lastSnapshot.latitude]),
                fromLonLat([snapshot.longitude, snapshot.latitude])]),
            });
            lines.push(line);
          }
          lastSnapshot = snapshot;
        }
      }
      console.log(res.snapshots);
      this.features.addFeatures(features);
      this.lines.addFeatures(lines);
    }, error => {
      console.error(error);
    });

  }

  get distance(): number|undefined {
    if (this.snapshots === undefined) {
      return undefined;
    }
    return this.snapshots.map(s => s.distance).reduce((a, b) => Math.max(a, b), 0) / 1_000_000;
  }

  get runtime(): number|undefined {
    if (this.snapshots === undefined) {
      return undefined;
    }
    return this.snapshots.map(s => s.runTime).reduce((a, b) => Math.max(a, b), 0) / 60 / 1000;
  }
}

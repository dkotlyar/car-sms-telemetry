import { Injectable } from '@angular/core';
import {Observable} from "rxjs";
import {map} from "rxjs/operators";
import {RestService} from "./rest.service";
import {LoggerService} from "./logger.service";
import {Snapshot} from "../classes/DAO/snapshot";
import {Paginator} from "../classes/paginator";

@Injectable({
  providedIn: 'root'
})
export class DjangoBackendService {

  constructor(private rest: RestService,
              private logger: LoggerService) { }

  // Получить список всех инспекций (с фильтрами)
  snapshots(page: number, perPage: number, filters: any, sort: string = ''): Observable<{snapshots:Snapshot[],pages:Paginator}> {
    let imei: string = filters.imei ?? '';
    let session: string = filters.session?? '';

    return this.rest.get('telemetries', {page, per_page: perPage, imei, session})
      .pipe(map(result => {
        const res = [];
        let pages = new Paginator();
        if (result.telemetries !== undefined) {
          for (const item of result.telemetries) {
            const obj = Snapshot.from_django(item)
            if (obj !== undefined) {
              res.push(obj);
            }
          }
        }
        if (result.pages !== undefined) {
          pages = Paginator.from_django(result.pages);
        }
        return {snapshots: res, pages};
      }));
  }
}

import { Injectable } from '@angular/core';
import {HttpClient, HttpHeaders} from "@angular/common/http";
import {Observable} from "rxjs";
import {map} from "rxjs/operators";
import {LoggerService} from "./logger.service";
import {environment} from "../../environments/environment";

@Injectable({
  providedIn: 'root'
})
export class RestService {

  constructor(private http: HttpClient,
              private logger: LoggerService) { }

  post(method: string, params: any): Observable<any> {
    return this.request('POST', method, params);
  }

  put(method: string, params: any): Observable<any> {
    return this.request('PUT', method, params);
  }

  get(method: string, params: any = {}): Observable<any> {
    return this.request('GET', method, params);
  }

  request(httpMethod: 'POST'|'GET'|'PUT', method: string, params: any): Observable<any> {
    if (!method.startsWith('/')) {
      method = '/' + method;
    }
    this.logger.info('Call rest method ', httpMethod, method, 'with params', params);
    const url = environment.apiUrl + method;
    const options: {headers:HttpHeaders,withCredentials:boolean,params:any|undefined,body:any|undefined} = {
      body: undefined, params: undefined,
      headers: new HttpHeaders({
        'Content-Type': 'application/json; charset=UTF-8'
      }),
      withCredentials: true
    };
    if (httpMethod === 'GET') {
      options['params'] = params;
    } else {
      options['body'] = params;
    }

    if (httpMethod === 'PUT' && params.id === undefined) {
      this.logger.warn('In rest PUT method must be `id` field in object.');
    }

    return this.http.request(httpMethod, url, options).pipe(
        map((value: any) => {
          if (value.error_code !== undefined && value.error_code > 0) {
            this.logger.error('Rest error', value, 'method', httpMethod, method, 'with params', params);
            throw value;
          }
          this.logger.info('Rest response', value, 'method', httpMethod, method, 'with params', params);
          return value;
        })
      );
  }
}

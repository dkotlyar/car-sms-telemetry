import {EventEmitter, Injectable} from '@angular/core';
import {environment} from "../../environments/environment";

@Injectable({
  providedIn: 'root'
})
export class LoggerService {

  private eventId = 0;

  constructor() {
  }

  info(...args: any[]) {
    if (!environment.production) {
      console.warn('INFO', ...args);
    }
  }

  warn(...args: any[]) {
    if (!environment.production) {
      console.warn('WARN', ...args);
      // this.event.emit(this.getToast(...args));
    }
  }

  error(...args: any[]) {
    if (!environment.production) {
      console.error('ERROR', ...args);
      // this.event.emit(this.getToast(...args));
    }
  }
}

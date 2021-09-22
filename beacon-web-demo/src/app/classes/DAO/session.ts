import {Car} from "./car";

export class Session {
  id: number|undefined;
  car: Car|undefined;
  sessionDatetime: Date|undefined;
  createDatetime: Date|undefined;
  updateDatetime: Date|undefined;

  static fromDjango(obj: any): Session|undefined {
    if (obj === undefined || obj == null) {
      return undefined;
    }
    const session = new Session();
    session.id = obj.id;
    session.car = Car.fromDjango(obj.car);
    session.sessionDatetime = new Date(obj.session_datetime);
    session.createDatetime = new Date(obj.create_datetime);
    session.updateDatetime = new Date(obj.update_datetime);
    return session;
  }
}

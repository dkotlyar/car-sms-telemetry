import {getOriginalReferences} from "@angular/compiler-cli/src/transformers/compiler_host";

export class Car {
  id: number|undefined;
  imei: string|undefined;
  gosNomer: string|undefined;
  brand: string|undefined;
  model: string|undefined;
  createDatetime: Date|undefined;
  updateDatetime: Date|undefined;

  static fromDjango(obj: any): Car|undefined {
    if (obj === undefined || obj === null) {
      return undefined;
    }
    const car = new Car();
    car.id = obj.id;
    car.imei = obj.imei;
    car.gosNomer = obj.gos_nomer;
    car.brand = obj.brand;
    car.model = obj.model;
    car.createDatetime = new Date(obj.create_datetime);
    car.updateDatetime = new Date(obj.update_datetime);
    return car;
  }

  get name(): string {
    var name_arr = [this.brand, this.model, this.gosNomer];
    return name_arr.join(' ')
  }
}

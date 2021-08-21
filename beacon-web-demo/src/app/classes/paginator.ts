export class Paginator {
  hasOtherPages: boolean = false;
  nextPageNumber: number|null = null;
  previousPageNumber: number|null = null;
  pageRange: number[] = [];
  number: number = 1;

  static from_django(obj: any) {
    const pages = new Paginator();
    pages.nextPageNumber = obj.next_page_number;
    pages.previousPageNumber = obj.previous_page_number;
    pages.pageRange = obj.page_range;
    pages.hasOtherPages = obj.has_other_pages;
    pages.number = obj.number;
    return pages;
  }
}

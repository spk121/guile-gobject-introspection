(define-module (empty-window empty-app-window)
  #:use-module (gi)
  #:use-module (gi gtk-3)
  #:export (empty-app-window-new))

(define <EmptyAppWindow>
  (register-type
   "EmptyAppWindow"                     ; type name
   <GtkApplicationWindow>               ; parent_type
   #f                                   ; No additional properties
   #f                                   ; No new signals
   #f))                                 ; No disposer func

(define (empty-app-window-new app)
  (make-gobject
   <EmptyAppWindow>
   `(("application" . ,app))))
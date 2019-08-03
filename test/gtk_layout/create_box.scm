(use-modules (gi) (gi repository)
             (srfi srfi-26)
             (rnrs bytevectors)
             (test automake-test-lib))

(unless (false-if-exception (require "Gtk" "3.0"))
  (exit 'EXIT_SKIPPED))

(for-each (cute load-by-name "Gtk" <>) '("init" "Box" "Orientation"))

(automake-test
 (begin
   (init)
   (let* ([box1 (box:new (make <GtkOrientation> 'vertical) 1)]
          [box2 (make <GtkBox>
                  #:orientation (make <GtkOrientation> 'vertical)
                  #:spacing 2)])
     (format #t "box 1: ~S~%" box1)
     (format #t "box 1 spacing: ~S~%" (spacing box1))
     (format #t "box 2: ~S~%" box2)
     (format #t "box 2 spacing: ~S~%" (spacing box2))
     (and (is-a? box1 <GtkBox>)
          (= 1 (spacing box1))
          (is-a? box2 <GtkBox>)
          (= 2 (spacing box2))))))

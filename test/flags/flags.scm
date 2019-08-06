(define-module (test flags flags)
  #:use-module (gi types)
  #:use-module (oop goops)
  #:use-module (ice-9 hash-table)
  #:re-export (<GFlags>
               flags->number flags->list number->flags list->flags flags-set?
               flags-mask flags-union flags-intersection flags-difference
               flags-complement flags-projection flags-projection/list
               flags-projection/number)
  #:export (<Flags> <OtherFlags>))

(define-class <Flags> (<GFlags>))
(define-class <OtherFlags> (<GFlags>))

(class-slot-set! <Flags> 'obarray
                 (alist->hashq-table
                  '((a . 1)
                    (b . 2)
                    (ab . 3)
                    (c . 4)
                    (d . 8))))

(class-slot-set! <OtherFlags> 'obarray
                 (alist->hashq-table
                  '((a . 16)
                    (b . 8)
                    (c . 2)
                    (d . 1)
                    (cd . 3))))

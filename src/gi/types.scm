;; Copyright (C) 2019 Michael L. Gran

;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.

(define-module (gi types)
  #:use-module (oop goops)
  #:use-module (srfi srfi-1)
  #:use-module (srfi srfi-2)
  #:use-module (system foreign)
  #:use-module (gi oop)

  #:export (<GIBaseInfo>
            enum->number enum->symbol number->enum symbol->enum
            flags->number flags->list number->flags list->flags flags-set?
            flags-mask flags-union flags-intersection flags-difference
            flags-complement flags-projection flags-projection/list
            flags-projection/number))

(eval-when (expand load eval)
  ;; this library is loaded before any other, so init logging here
  (load-extension "libguile-gi" "gig_init_logging")
  (load-extension "libguile-gi" "gig_init_types"))

;;; Enum conversions

(define-method (enum->symbol (enum <GEnum>))
  (let ((expected (slot-ref enum 'value)))
    (hash-fold (lambda (key value seed)
                 (if (= value expected) key seed))
               #f (slot-ref enum 'obarray))))

(define-method (enum->symbol (class <class>) (enum <GEnum>))
  (if (is-a? enum class)
      (enum->symbol enum)
      (scm-error 'wrong-type-arg "enum->symbol" "expected ~a"
                 (list (class-name class)) (list enum))))

(define-method (enum->symbol (class <class>))
  (lambda (enum) (enum->symbol class enum)))

(define-method (enum->number (number <number>))
  (format (current-error-port) "WARNING: passing number ~a as enum~%" number)
  (display-backtrace (make-stack #t 1) (current-error-port))
  number)

(define-method (enum->number (enum <GEnum>))
  (slot-ref enum 'value))

(define-method (enum->number (class <class>) (enum <GEnum>))
  (if (is-a? enum class)
      (enum->number enum)
      (scm-error 'wrong-type-arg "enum->number" "expected ~a"
                 (list (class-name class)) (list enum))))

(define-method (enum->number (class <class>) (symbol <symbol>))
  (hashq-ref (class-slot-ref class 'obarray) symbol))

(define-method (enum->number (class <class>))
  (lambda (symbol) (enum->number class symbol)))

(define-method (symbol->enum (class <class>) (symbol <symbol>))
  (or (and-let* ((obarray (class-slot-ref class 'obarray))
                 (value (hashq-ref obarray symbol)))
        (make class #:value value))
      (scm-error 'out-of-range "symbol->enum"
                 "not defined in ~A" (list class) (list symbol))))

(define-method (symbol->enum (class <class>))
  (lambda (symbol) (symbol->enum class symbol)))

(define-method (number->enum (class <class>) (number <number>))
  (or
   (hash-fold
    (lambda (key value seed)
      (if (= value number) (make class #:value value) seed))
    #f (class-slot-ref class 'obarray))
   (scm-error 'out-of-range "number->enum"
              "not bound in ~A" (list class) (list symbol))))

(define-method (number->enum (class <class>))
  (lambda (number) (number->enum class number)))

;;; Flag conversions

(define-method (flags->number (number <number>))
  (format (current-error-port) "WARNING: passing number ~a as flags~%" number)
  (display-backtrace (make-stack #t 1) (current-error-port))
  number)

(define-method (flags->number (flags <GFlags>))
  (slot-ref flags 'value))

(define-method (number->flags (class <class>) (number <number>))
  ((lambda (flags)
     (if (= (flags->number flags) number)
         flags
         (scm-error 'out-of-range "number->flags"
                    "strange flags seem set" '() (list number))))
   (make class
     #:value
     (hash-fold
      (lambda (key value seed)
        (if (= (logand number value) value)
            (logior value seed)
            seed))
      0 (class-slot-ref class 'obarray)))))

(define-method (number->flags (class <class>))
  (lambda (number) (number->flags class number)))

(define-method (flags-set? (flags <GFlags>) (number <number>))
  (= (logand (slot-ref flags 'value) number) number))

(define-method (flags-set? (flags <GFlags>) (symbol <symbol>))
  (flags-set? flags (hashq-ref (slot-ref flags 'obarray) symbol)))

(define-method (flags-set? (flags <GFlags>) (list <list>))
  (every (lambda (f) (flags-set? flags f)) list))

(define-method (flags->list (flags <GFlags>))
  (hash-fold
   (lambda (key value seed)
     (if (flags-set? flags value) (cons key seed) seed))
   '() (slot-ref flags 'obarray)))

(define-method (flags->list (class <class>) (flags <GFlags>))
  (if (is-a? flags class)
      (flags->list flags)
      (scm-error 'wrong-type-arg "flags->list" "expected ~a"
                 (list (class-name class)) (list flags))))

(define-method (flags->list (class <class>))
  (lambda (flags) (flags->list class flags)))

(define-method (list->flags (class <class>) (list <list>))
  (let* ((obarray (class-slot-ref class 'obarray))
         (lookup (lambda (symbol)
                   (or (hashq-ref obarray symbol)
                       (scm-error 'out-of-range "list->flags"
                                  "not defined in ~A" (list class)
                                  (list symbol))))))
    (make class #:value (apply logior (map lookup list)))))

(define-method (list->flags (class <class>))
  (lambda (flags) (list->flags class flags)))

(define-method (flags->number (class <class>) (flags <GFlags>))
  (if (is-a? flags class)
      (flags->number flags)
      (scm-error 'wrong-type-arg "flags->number" "expected ~a"
                 (list (class-name class)) (list flags))))

(define-method (flags->number (class <class>) (list <list>))
  (flags->number (list->flags class list)))

(define-method (flags->number (class <class>))
  (lambda (list) (flags->number class list)))

;;; rnrs enums analogues

(define-method (flags-mask (class <class>))
  (make class #:value
        (hash-fold (lambda (key value seed) (logior value seed)) 0
                   (class-slot-ref class 'obarray))))

(define-method (flags-mask (flags <GFlags>))
  (flags-mask (class-of flags)))

(define-method (flags-union (flags1 <GFlags>) (flags2 <GFlags>))
  (if (equal? (class-of flags1) (class-of flags2))
      (make (class-of flags1) #:value
            (logior (slot-ref flags1 'value)
                    (slot-ref flags2 'value)))
      (error "cannot unite flags of differing type")))

(define-method (flags-union (car <GFlags>) . rest)
  (fold flags-union car rest))

(define-method (flags-intersection (flags1 <GFlags>) (flags2 <GFlags>))
  (if (equal? (class-of flags1) (class-of flags2))
      (make (class-of flags1) #:value
            (logand (slot-ref flags1 'value)
                    (slot-ref flags2 'value)))
      (error "cannot unite flags of differing type")))

(define-method (flags-intersection (car <GFlags>) . rest)
  (fold flags-intersection car rest))

(define-method (flags-difference (flags1 <GFlags>) (flags2 <GFlags>))
  (if (equal? (class-of flags1) (class-of flags2))
      (make (class-of flags1) #:value
            (logxor (slot-ref flags1 'value)
                    (slot-ref flags2 'value)))
      (error "cannot unite flags of differing type")))

(define-method (flags-complement (flags <GFlags>))
  (flags-difference flags (flags-mask flags)))

(define-method (flags-projection/list (flags <GFlags>) (class <class>))
  (let ((v (flags->list flags))
        (m (flags->list (flags-mask class))))
    (list->flags class (lset-intersection eq? v m))))

(define-method (flags-projection/list (flags <GFlags>) (flags2 <GFlags>))
  (flags-projection/list flags (class-of flags2)))

(define-method (flags-projection/number (flags <GFlags>) (class <class>))
  (let ((v (flags->number flags))
        (m (flags->number (flags-mask class))))
    (number->flags class (logand v m))))

(define-method (flags-projection/number (flags <GFlags>) (flags2 <GFlags>))
  (flags-projection/number flags (class-of flags2)))

(define flags-projection flags-projection/list)

;;; Enum/Flag printing

(define-method (display (enum <GEnum>) port)
  (display (enum->symbol enum) port))

(define-method (display (flags <GFlags>) port)
  (display (flags->list flags) port))

(define-method (write (enum <GEnum>) port)
  (format port "#<~s ~a>" (class-name (class-of enum)) (enum->symbol enum)))

(define-method (write (flags <GFlags>) port)
  (format port "#<~s ~a>" (class-name (class-of flags)) (flags->list flags)))

;;; Enum equality

(define-method (= (enum1 <GEnum>) (enum2 <GEnum>))
  (eq? (slot-ref enum1 'value) (slot-ref enum2 'value)))

(define-method (= (enum <GEnum>) (number <number>))
  (= (enum->number enum) number))

(define-method (= (number <number>) (enum <GEnum>))
  (= number (enum->number enum)))

(define-method (equal? (enum1 <GEnum>) (enum2 <GEnum>))
  (and (equal? (class-of enum1) (class-of enum2))
       (eq? (slot-ref enum1 'value) (slot-ref enum2 'value))))

;;; Flag equality

(define-method (= (flags <GFlags>) (number <number>))
  (= (slot-ref flags 'value) number))

(define-method (= (number <number>) (flags <GFlags>))
  (= number (slot-ref flags 'value)))

(define-method (= (flags1 <GFlags>) (flags2 <GFlags>))
  (= (slot-ref flags1 'value) (slot-ref flags2 'value)))

(define-method (equal? (flags1 <GFlags>) (flags2 <GFlags>))
  (and (equal? (class-of flags1) (class-of flags2))
       (= flags1 flags2)))

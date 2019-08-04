;; Copyright (C), 2019 Michael L. Gran

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
  #:use-module (system foreign)
  #:use-module (gi oop)

  #:export (<GIBaseInfo>
            enum->number
            flags->number
            number->enum
            number->flags))

(eval-when (expand load eval)
  ;; this library is loaded before any other, so init logging here
  (load-extension "libguile-gi" "gig_init_logging")
  (load-extension "libguile-gi" "gig_init_types"))

(define-method (enum->number (number <number>))
  (format (current-error-port) "WARNING: passing number ~a as enum~%" number)
  (display-backtrace (make-stack #t 1) (current-error-port))
  number)

(define-method (enum->number (enum <GEnum>))
  (let ((value (slot-ref enum 'value))
        (obarray (slot-ref enum 'obarray)))
    (hashq-ref obarray value)))

(define-method (enum->number (class <class>) (symbol <symbol>))
  (enum->number (make class #:value symbol)))

(define-method (number->enum-value (class <class>) (number <number>))
  (hash-fold (lambda (key value seed)
               (if (= value number) key seed))
             0 (class-slot-ref class 'obarray)))

(define-method (number->enum (class <class>) (number <number>))
  (make class #:value (number->enum-value class number)))

(define-method (number->enum (class <class>))
  (lambda (number) (number->enum class number)))

(define-method (flags->number (number <number>))
  (format (current-error-port) "WARNING: passing number ~a as flags~%" number)
  (display-backtrace (make-stack #t 1) (current-error-port))
  number)

(define-method (flags->number (flags <GFlags>))
  (let ((value (slot-ref flags 'value))
        (obarray (slot-ref flags 'obarray)))
    (apply logior
           (map (lambda (v) (hashq-ref obarray v 0))
                value))))

(define-method (flags->number (class <class>) (list <list>))
  (flags->number (make class #:value list)))

(define-method (number->flags-value (class <class>) (number <number>))
  (hash-fold (lambda (key value seed)
               (if (logtest value number) (cons key seed) seed))
             '() (class-slot-ref class 'obarray)))

(define-method (number->flags (class <class>) (number <number>))
  (make class #:value (number->flags-value class number)))

(define-method (number->flags (class <class>))
  (lambda (number) (number->flags class number)))

(define-method (display (enum <GEnum>) port)
  (display (slot-ref enum 'value) port))

(define-method (display (flags <GFlags>) port)
  (display (slot-ref enum 'value) port))

(define-method (write (enum <GEnum>) port)
  (format port "#<~s ~a>" (class-name (class-of enum)) (slot-ref enum 'value)))

(define-method (write (flags <GFlags>) port)
  (format port "#<~s ~a>" (class-name (class-of flags)) (slot-ref flags 'value)))

(define-method (= (enum1 <GEnum>) (enum2 <GEnum>))
  (eq? (slot-ref enum1 'value) (slot-ref enum2 'value)))

(define-method (= (enum <GEnum>) (number <number>))
  (= (enum->number enum) number))

(define-method (= (number <number>) (enum <GEnum>))
  (= number (enum->number enum)))

(define-method (equal? (enum1 <GEnum>) (enum2 <GEnum>))
  (and (equal? (class-of enum1) (class-of enum2))
       (eq? (slot-ref enum1 'value) (slot-ref enum2 'value))))

(define-method (= (flags <GFlags>) (number <number>))
  (= (flags->number flags) number))

(define-method (= (number <number>) (flags <GFlags>))
  (= number (flags->number flags)))

(define-method (= (flags1 <GFlags>) (flags2 <GFlags>))
  (= (flags->number flags1) (flags->number flags2)))

(define-method (equal? (flags1 <GFlags>) (flags2 <GFlags>))
  (and (equal? (class-of flags1) (class-of flags2))
       ;; compare with =, because flags need not be canonical
       (= flags1 flags2)))

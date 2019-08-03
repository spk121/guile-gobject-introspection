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
            flags->number))

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

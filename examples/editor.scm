;; Copyright (C) 2019  Michael L. Gran
;; Copyright (C) 2019  Jan (janneke) Nieuwenhuizen <janneke@gnu.org>

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
(use-modules (gi)
             (gi repository)
             (gi types)
             (gi util)
             (oop goops describe)
             (ice-9 receive))

(push-duplicate-handler! 'merge-generics)
(use-typelibs
 (("Gio" "2.0") #:renamer (protect* '(application:new receive)))
 ("Gtk" "3.0")
 ("Gdk" "3.0"))

(define (print-goodbye widget)
  (display "Goodbye World\n"))

(define (key-press widget event)
  (receive (ok keyval)
      (get-keyval event)
    (format #t "key: ~s\n" keyval)
    #f))

(define (activate app)
  (let ((window (application-window:new app))
        (grid (grid:new))
        (editor (text-view:new))
        (button-box (button-box:new (symbol->orientation 'horizontal)))
        (button (button:new-with-label "Quit"))
        (button2 (button:new-with-label "Hello"))
        (key-press-mask (list->event-mask '(key-press-mask))))
    (set-vexpand editor #t)
    (set-hexpand editor #t)
    (add-events editor (flags->number key-press-mask))
    (attach grid editor 0 0 1 1)

    (set-layout button-box (symbol->button-box-style 'end))
    (add button-box button2)
    (add button-box button)
    (attach grid button-box 0 1 1 1)

    (add window grid)
    (set-title window "Window")
    (set-default-size window 200 200)

    (connect editor (make <signal> #:name "key-press-event") key-press)
    (connect button clicked print-goodbye)
    (connect button clicked (lambda x (destroy window)))
    (connect button2 clicked
             (lambda x
               (let ((buffer (get-buffer editor))
                     (iter1 (make <GtkTextIter>))
                     (iter2 (make <GtkTextIter>)))
                 (get-bounds! buffer iter1 iter2)
                 (write (get-text buffer iter1 iter2 #t))
                 (newline)
                 (set-text buffer "Hello, world" 12))))

    (grab-focus editor)
    (show-all window)))

(define (main)
  (let ((app (application:new "org.gtk.example" (number->application-flags 0))))
    (connect app application:activate activate)
    (run app (command-line))))

(main)

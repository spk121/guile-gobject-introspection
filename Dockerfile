FROM ubuntu:eoan

WORKDIR /app

COPY . /app

ENV LANG C.UTF-8
ENV GUILD /usr/bin/guild
ENV TERM dumb
ENV VERBOSE true

RUN apt-get update \
  && apt-get install -y libffi-dev gir1.2-glib-2.0 libgirepository1.0-dev \
  guile-2.2-dev meson texinfo gnulib git \
  texlive-base texlive-plain-generic texlive-latex-base


# CMD ["autoreconf -vif -Wall && ./configure --enable-hardening && make && make check"]

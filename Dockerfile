FROM ubuntu:latest

WORKDIR /app

COPY . /app

ENV LANG C.UTF-8
ENV GUILD /usr/bin/guild
ENV TERM dumb

RUN apt-get update \
  && apt-get install -y libffi-dev gir1.2-glib-2.0 libgirepository1.0-dev \
  guile-2.2-dev libtool texinfo autoconf automake gnulib git texlive-full


# CMD ["autoreconf -vif -Wall && ./configure --enable-hardening && make && make check"]

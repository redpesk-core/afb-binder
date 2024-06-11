# Running HTTPS

The HTTPS secured protocol of the HTTP server is activated by the
option --https.

The options --https-cert and --htps-key can be used to tell
the binder what file to use for its certificate and private key.

## Quick start with HTTPS

To run an HTTPS instance of **afb-binder** you need a private key
and a certificate.

If you don't have one, you can create a self signed one for testing.
This can be done using OPENSSL in the following way:

```
openssl req -x509 -days 30 -newkey rsa:4096 -nodes -keyout key.pem -out cert.pem
```

That creates in the current directory the private key file 'key.pem' and
the self-signed certificate file 'cert.pem' attached to that key.
The certificate is avalaible for 30 days (see option -days).

Having the certificate and the key, the command to run the binder is:

```
afb-binder --https --https-key key.pem --https-cert cert.pem
```

You can connect to that binder with a browser at **https://localhost:1234/**.

Because the certificate is self signed, your browser will
complain and emit warnings.

## Settings of HTTPS

The binder start HTTPS if one of the following condition is met:

1. the option **--https** is set
2. the environment variable **AFB_HTTPS** has one of the following values:
1, true, on, yes

If the HTTPS is started, the binder will search for the key
and the certificate files.

That search is done using the following order:

1. the option if set (**--https-key** or **--https-cert**)
2. the environment variable if set (**AFB_HTTPS_CERT** or **AFB_HTTPS_KEY**)
3. the files **X**key.pem or **X**cert.pem where
   **X** is the value of the environment variable **AFB_HTTPS_PREFIX** if set,
   or /etc/afb/.https/



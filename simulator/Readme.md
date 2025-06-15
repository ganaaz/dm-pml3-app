
# To Genreate local development certificate

openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes
openssl pkcs12 -export -out cert.pfx -inkey key.pem -in cert.pem -passout pass:yourpassword


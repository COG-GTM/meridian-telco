CREATE ROLE inventory LOGIN PASSWORD 'inventory';
CREATE ROLE billing   LOGIN PASSWORD 'billing';
CREATE ROLE network   LOGIN PASSWORD 'network';
CREATE DATABASE inventory OWNER inventory;
CREATE DATABASE billing   OWNER billing;
CREATE DATABASE network   OWNER network;

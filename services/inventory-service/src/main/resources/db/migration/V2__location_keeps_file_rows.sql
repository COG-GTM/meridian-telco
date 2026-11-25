-- locations.cpp load_locations keeps every row of data/locations.csv in file order, so a repeated
-- LOC_CD is listed twice by /capacity. Key the table on the file position instead of LOC_CD.
ALTER TABLE location DROP CONSTRAINT location_pkey;
ALTER TABLE location ADD PRIMARY KEY (seq);
CREATE INDEX location_loc_cd ON location (loc_cd);

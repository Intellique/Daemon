DROP TABLE IF EXISTS Archive, Archive_to_checksum_result, Archive_volume, Archive_volume_to_checksum_result, Changer, Changer_slot, Checksum, Checksum_result, Drive, Drive_format, Drive_format_support, File_to_archive_volume, File_to_checksum_result, File_set, File, Host, Job, Job_record, Pool, Tape, Tape_format CASCADE;
DROP TYPE IF EXISTS Changer_status, Drive_status, File_type, Job_status, Job_type, Tape_format_dataType, Tape_format_mode, Tape_location, Tape_status CASCADE;

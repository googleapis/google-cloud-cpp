# Sample Queries for DB

```{sql}
WITH dups AS (
SELECT size
     , crc32c
     , COUNT(*) AS copies
  FROM gcs_objects
 WHERE bucket = 'coryan-test-large-bucket-1000000'
 GROUP BY size, crc32c
HAVING copies > 1
  )
SELECT o.bucket
     , o.object
     , o.size
     , o.crc32c
     , o.md5_hash
  FROM gcs_objects AS o, dups AS d
 WHERE o.bucket = 'coryan-test-large-bucket-1000000'
   AND o.size = d.size
   AND o.crc32c = d.crc32c
 ORDER BY o.crc32c, o.size
     ;
```

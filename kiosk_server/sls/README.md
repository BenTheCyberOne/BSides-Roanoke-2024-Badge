# Crap I installed here

* Poetry to manage python bits
* Serverless framework for deployment


## First time install (future reference)

* npm install all the junk
* Install AWS Keys

```
serverless create-cert
serverless create_domain
```

## SLS resources

###  Dynamo Schema

Two tables.  

* users2024 - Users to track badge_id, username, team, scores, etc. Should be 
  small in size to enable scanning
* badges2024 - Badge table.  Basically the bulk upload, and only indexed by 
  badge. Used for auth and verification.  Might be pre-populated.


### Endpoints

All endpoints end in badge.bsidesroa.org

* 2024-bucket
    * Static hosting
* 2024-api
    * Api
* 2024
    * Primary webpoint for app.  Points to cloudfront, which fronts the
      2024-bucket


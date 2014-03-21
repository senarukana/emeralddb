# Emeralddb
Emeralddb is a json document nosql database like mongodb. It's based on BSON and boost. Currently it only supports k/v operations. The internal data is managed by mmap. It supports Hash-based and BTree-base index on _id. 

## Limitations
1. It doesn't support intuitive query language. It makes it really hard to use.
2. The database is really a trial and shouln't be used in real case.  

## Geting started
1. Make sure you have boost.
2. Modify the Makefile.am to change the path of it. The default path of boost if in the parent path of src.
3. Run ./build.sh
If it has some problems, may be your versions of boost are incompatible with the version of bson. My boost version is 1.54.

## Operation API
After making it, it will have a client program named edb and server program named emberalddb. Use it for test. You need firstly to run the emeralddb. 
You can type help for command usages
	
	./edb
	edb>> help
### Connect
Before executing operations, you need connect the server first. The default port is 48127.

	edb>> connect localhost:48127
It will return +OK to if your connection has been built sucessful.

### Database
The first time, you use it you need to create a database.

	edb>> create test

Then you can type show databases to see how many databases you have.
	
	edb>> show databases
	# delete database test
	edb>> drop test 

The database will be set to null by default. You need to specify it before executing any operations.
	
	edb>> use test 

### Insert
Every data has an _id internally. Currently, You can specify the _id to increment or randomly.
The structure of data is JSON.  
	
	# default _id
	edb>> insert {"name":"lizhe", "age":"30"}
it will return the _id to show you the id
	
	edb>> +OK _id = 1341409
	
	# specify _id
	edb>> insert {“_id”:1, "email": "lizhe.ted@gmail.com"}
	# return
	edb>> +OK _id=1

### Delete
Without index, you can only delete data by _id.
	
	edb>> delete {"_id":"1"}
	# return
	edb>> +OK

If you have an index on email, you can type:
	
	edb>> delete {"email":"lizhe.ted@gmail.com"}
	
	# you can also use wildcard expression for deletion
	edb>> delete {"email": "lizhe*"}
	

### Query
	edb>> query {"_id":"1"}
	# return
	edb>> {"id":"1","email": "lizhe.ted@gmail.com"}
	
	edb>> query {"_id", "2"}
	# return
	edb>> NULL

### Create index

#### Limitation
1. index can only contain one column. 
2. the column value should be unique.
Very sad, isn't it? come to help me!
 
You can specify the index type in creating index. Default is hash-base index.

command format is:
	
	create index on [database] (name [AESC|DESC]) [hash|btree]
	
	edb>> create index on test (email) btree

## Log
The system log is stored in [dialog.log]

The operation log is stored in [oper.log].

Currently the operation log is handled very badly. It stores all the data in one file and will not rotate automatically.

## Contact
If you have problems, fell free to contact me at any time. My email is: lizhe.ted@gmail.com

If you have interest in developing this project, just fork it, and do what you want.



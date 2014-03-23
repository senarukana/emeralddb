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

## Driver
Currently it only supports C++ and Java. The java driver is in the /driver/java. If you want to test it in distributed system, you can configure the client with Ketama hashing. 
# Internal
## Protocol
The protocol is really trivial. Every message begins with a msg header. It begins with a msglen, and a type to indicate the type of msg. For simplification, both of them is stored formated with int32. the format is like:

	********MsgHeader*****
	=====================
	|		Length 		 |
	=====================
	| 		Type		 |
	=====================

Currently, it has the following types.
	
	#define OP_REPLY                   1
	#define OP_INSERT                  2
	#define OP_MULTI_INSERT            3
	#define OP_DELETE                  3
	#define OP_QUERY                   4 
	#define OP_CREATE_SCHEME           16
	#define OP_DROP_SCHEME             17
	#define OP_CREATE_INDEX            18
	#define OP_DROP_INDEX              19
	#define OP_CONNECT                 32
	#define OP_DISCONNECT              33
	#define OP_SNAPSHOT                33
For client, the remaining msg can be the data you want to insert. The idea is simple, if it has only one element, the remainng msg should only be your data. If it has multiple elements, it starts with a int to indicate the number of elements. And every elements starts with a int to indicate the length of it.
For example the multiple insert should be like:
	
	============================
	|		MSGHEADER			|
	============================
	|		elments_num 		|
	============================
	|		element1_len		|
	============================
	|		element1_data		|
	============================
	|		element2_len		|
	===========================
	|		element2_data		|
	............................
	
## Storage structure
The internal storage structure is a little bit like mysql, but it's much more easy. It has 4 kinds of structure to manage the data.

#### DATABASE STRUCTURE #######
It firstly reads this header before openning the database. It contains the basic information of database. Magic is to verify the header. Flag indicate if the database is closed rightly the last time. Scheme Num is the scheme currently the database have. And the remaining is the name of each scheme. We use scheme name to locate the scheme file.
   
	/********************************
		DATABASE STRUCTURE
	=============================
	|           Magic           |
	=============================
	|     		Size            |
	=============================
	|        	Flag            |
	=============================
	|        	Version         |
	=============================
	|         SCHEME LIST        |
	=============================
	.............................
	*********************************/

#### SCHEME STRUCTURE #######
It contains the basic information of scheme. It has a fixed size of 4096.
   
	/********************************
		  SCHEME STRUCTURE
	=============================
	|           Magic           |
	=============================
	|     	 Page Num           |
	=============================
	|        Record Num         |
	=============================
	|	INDEX LIST 	    |
	.............................
	*********************************/

Index structure stores the information of index. Currently it only supports BTREE Index and LinearHash index, and one to one index.

	*********************
		INDEX STRUCTRE
	=========================
	|	Type 		|
	=========================
	| 	  Field Num	|
	=========================
	|	  Field Name 1  | (64char)
	=========================
	|	  Field Name 2  |
	.........................

#### PAGE STRUCTURE ##########
The data is managed by page using mmap. Page has a fixed size of 4096. Every time the database read the data, it should allocate the whole page to the memory. The data in the page is stored reversely from end to begin. In this way, we can easily check if the page is full. Slot is the index of record in this page. 

	*****************************
	      PAGE STRUCTURE
	=============================
	|       PAGE HEADER         |
	=============================
	|       Slot List           |
	=============================
	|       Free Space          |
	=============================
	|       Data                |
	=============================

When the space is full, we will extend the database. At this moment, we will not extend only one page(too small), we will use a term SEGMENT, which contains a bunch of pages, currently it configures with 64. 

#### RECORD STRUCTURE ###########
Using both PAGEID and SLOTID to locate the exact record.

	*****************************
	       RECORD STRUCTURE
	=============================
	|           PAGEID          |
	=============================
	|           SLOTIT          |
	=============================


## Contact
If you have problems, fell free to contact me at any time. My email is: lizhe.ted@gmail.com

If you have interest in developing this project, just fork it, and do what you want.
	|       Data                |
	==============================
	*********************************/

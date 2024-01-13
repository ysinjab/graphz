#include "redismodule.h"
#include <string.h>

/* Check if a node exists in a dictionary graph. 
 * It returns 1 if the node does not exist, or 0 if the node exists. 
*/
int nodeExists(RedisModuleDict *dictGraph, RedisModuleString *node) {
  int nokey;
  RedisModule_DictGet(dictGraph, node, &nokey);
  return nokey;
}

/* Visit a node and all of the nodes in its adjacency list.
 * It returns 0 if the visit was successful. 
 * It returns 1 when error happened.
 */
int visit(RedisModuleCtx *ctx, RedisModuleDict *visited, RedisModuleString *node) {
  if (nodeExists(visited, node) == 0) {
    return 0;
  }

  RedisModuleCallReply *reply = RedisModule_Call(ctx, "HGETALL", "s", node);
  if (reply == NULL){
    RedisModule_ReplyWithError(ctx, "ERR reply is NULL");
    return 1;
  }

  size_t replyLen = RedisModule_CallReplyLength(reply);
  int degree = replyLen / 2;

  // prepare an array for the passed node's adjacent nodes
  char **adjacentNodes;
  adjacentNodes = malloc(degree * sizeof(char *));

  for (int i = 0; i < replyLen; i += 2) {
    RedisModuleCallReply *subreply;
    subreply = RedisModule_CallReplyArrayElement(reply, i);

    size_t len;
    char *key = RedisModule_Strdup(RedisModule_CallReplyStringPtr(subreply, &len));
    key[len] = 0;
    adjacentNodes[i / 2] = key;
    strcpy(adjacentNodes[i / 2], key);
  }
  RedisModule_FreeCallReply(reply);
  
  RedisModule_DictSet(visited, node, RedisModule_CreateStringFromLongLong(ctx, (long long)degree));
  for (int i = 0; i < degree; i++) {
    visit(ctx, visited, RedisModule_CreateString(ctx, adjacentNodes[i], strlen(adjacentNodes[i])));
  }
  return 0;
}


/* graphz.DFS [ADDEDGE <n>]
 *  Performs a depth-first search on the graph starting from node n.
 *  If the command receives "DFS <n>" it returns an array with the nodes visted and its degrees.
 *  Response format: [n1, degree1, n2, degree2, ...]
 */
int DFS(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  RedisModule_AutoMemory(ctx);
  if (argc != 2) return RedisModule_WrongArity(ctx);
  
  RedisModuleKey *key = RedisModule_OpenKey(ctx,argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_HASH &&
      type != REDISMODULE_KEYTYPE_EMPTY)
  {
      return RedisModule_ReplyWithError(ctx,REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  RedisModuleCallReply *reply = RedisModule_Call(ctx, "HGETALL", "s", argv[1]);
  if (reply == NULL) return RedisModule_ReplyWithError(ctx, "ERR reply is NULL");

  RedisModuleDict *visited = RedisModule_CreateDict(NULL);
  visit(ctx, visited, argv[1]);
 
  RedisModuleDictIter *iter = RedisModule_DictIteratorStartC(visited, "^", NULL, 0);
  char *nodeKey;
  size_t currentNodeKeyLen;
  RedisModuleString *degree;
  long long replylen = 0; /* Keep track of the emitted array len. */
  RedisModule_ReplyWithArray(ctx,REDISMODULE_POSTPONED_LEN);
  while ((nodeKey = RedisModule_DictNextC(iter, &currentNodeKeyLen, (void **) &degree)) != NULL) {
    RedisModule_ReplyWithStringBuffer(ctx, nodeKey, currentNodeKeyLen);
    RedisModule_ReplyWithString(ctx, degree);
    replylen++;
  }
  RedisModule_ReplySetArrayLength(ctx,replylen*2);
  RedisModule_DictIteratorStop(iter);
  RedisModule_FreeDict(ctx, visited);
  return REDISMODULE_OK;
}


/* graphz.ADDEDGE [ADDEDGE <n1> <n2>]
 *  Atomically add an edge between two nodes. If the edge already exists, the command does nothing.
 *  Edges are represented as a redis hash. This command will create two hashes, one for each node to represent directed edges. 
 *  The key of the hash is the node name, and the feilds are the names of the nodes it is connected to. Fields values are empty string.
 *  If the command receives "ADDEDGE <n1> <n2>" it returns OK if the edges were added.
 */
int AddEdge(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  RedisModule_AutoMemory(ctx);
  if (argc != 3) return RedisModule_WrongArity(ctx);

  RedisModuleKey *key1 = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  if (RedisModule_KeyType(key1) != REDISMODULE_KEYTYPE_HASH &&
      RedisModule_KeyType(key1) != REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  RedisModuleKey *key2 = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ | REDISMODULE_WRITE);
  if (RedisModule_KeyType(key2) != REDISMODULE_KEYTYPE_HASH &&
      RedisModule_KeyType(key2) != REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  RedisModule_HashSet(key1, REDISMODULE_HASH_NONE, argv[2], RedisModule_CreateString(ctx, "", 0), NULL);
  RedisModule_HashSet(key2, REDISMODULE_HASH_NONE, argv[1], RedisModule_CreateString(ctx, "", 0), NULL);
  RedisModule_ReplyWithSimpleString(ctx, "OK");
  return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {

  if (RedisModule_Init(ctx, "graphz", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "graphz.addedge", AddEdge, "write", 1, 1, 1) ==
      REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "graphz.dfs", DFS, "readonly", 1, 1, 1) ==
      REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return REDISMODULE_OK;
}
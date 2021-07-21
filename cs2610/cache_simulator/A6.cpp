#include <bits/stdc++.h>
using namespace std;
int cache_access = 0,read_access = 0,write_access = 0,cache_misses = 0,compulsory_misses = 0,capacity_misses = 0,conflict_misses = 0,read_misses = 0,write_misses = 0,dirty_blocks_evicted = 0;
set<int> compulsoryMissFinder;																	//This set has all previously accessed blocks,used to find compulsory and conflict misses
void writeToNextLevelCache()																	//Writes to next level cache(called when dirty block is evicted)
{
	dirty_blocks_evicted++;
	//dummy function for completion
}
struct cacheBlock																				//each block contains a tag, a valid bit and a dirty bit
{
	int tag;
	bool valid_bit;
	bool dirty_bit;
	cacheBlock()
	{
		valid_bit = false;
		dirty_bit = false;
	}
};
struct cache
{
	int associativity;																			//associativity of cache
	int setCnt;																					//number of sets in cache
	int tagLen;																					//length of tag
	int setIndLen;																				//set index length
	int blockOffsetLen;																			//block offset length
	int numBlocksInSet;																			//number of blocks in a set
	short int replacement_policy;																//replacement policiy used,0 for random 1 for lru , 2 for pseudo lru
	vector<vector<cacheBlock> > sets;															//This has all the cacheBlocks of the cache
	vector<vector<int> > recencyorder;															//This stores recency order and memory is assigned only when replacement policy is 1
	vector<vector<int> > lrutree;																//This stores nodes of tree, memory is assigned only when replacement policy is 2
	vector<int> validCnt;																		//This contains number of valid bits in a set
	int totValidCnt = 0;																		//This contains total number of valid bits. Used for capacity and conflict misses count
	/*
	Purpose:
	Constructor that allocates memory to elements and initializes required variables
	Parameter(s):
	<1> cacheSize : The size of cache in bytes
	<2> Block Size : Block size in bytes
	<3> assoc :		Associativity of cache
	<4> repl_pol :	Replacement policy used in cache
	returns: N/A
	*/
	cache(int cacheSize,int BlockSize,int assoc,int repl_pol)
	{

		blockOffsetLen = log2(BlockSize);
		int numOfBlocks = cacheSize/BlockSize;
		associativity = assoc;
		replacement_policy = repl_pol;
		if(associativity == 0)
		{
		 	sets.assign(1,vector<cacheBlock>(numOfBlocks));
		 	lrutree.assign(1,vector<int> (numOfBlocks-1,0));
		 	validCnt.assign(1,0);
		 	numBlocksInSet = numOfBlocks;
		 	recencyorder.resize(1);
		 	setCnt = 1;
		}
		else if(associativity == 1)
		{
			sets.assign(numOfBlocks,vector<cacheBlock> (1));
			lrutree.assign(numOfBlocks,vector<int> (1,0));
			validCnt.assign(numOfBlocks,0);
			recencyorder.resize(numOfBlocks);
			numBlocksInSet = 1;
			setCnt = numOfBlocks;
		}
		else
		{
			numBlocksInSet = associativity;
			setCnt = numOfBlocks/numBlocksInSet;
			sets.assign(setCnt,vector<cacheBlock> (numBlocksInSet));
			lrutree.assign(setCnt,vector<int> (numBlocksInSet-1,0));
			recencyorder.resize(setCnt);
			validCnt.assign(setCnt,0);
		}
		setIndLen = log2(setCnt);
		tagLen = 32 - setIndLen - blockOffsetLen;
	}
	/*
	Purpose:
	Helper function for read and write if replacement policy is pseudo LRU,called when there is a hit to update nodes to point away from hitIndex
	Parameter(s):
	<1> setIndex : setIndex of memory reference
	<2> hitIndex : The index in a set where the hit has occured
	returns: N/A
	*/
	void pseudoLRUHit(int setIndex,int hitIndex)
	{
		if(associativity == 1) return;
		int par = hitIndex/2;
		int child = hitIndex;
		int half = numBlocksInSet/2;
		while(par != numBlocksInSet-1)
		{
			if(child&1)
			{
				//right child,we should point it to left
				lrutree[setIndex][par] = 0;

			}
			else
			{
				//left child,we should point it to right
				lrutree[setIndex][par] = 1;
			}
			child = par;
			par = child/2 + half;
		}
	}
	/*
	Purpose:
	Helper function for read and write if replacement policy is pseudo LRU and a block eviction is required
	Parameter(s):
	<1> setIndex : set Index of memory reference
	<2> tag 	: 	Tag of memory reference
	<3> isRead 	: 	True if its a read and false if its a write
	returns: N/A
	*/
	void pseudoLRUreplace(int setIndex,int tag,bool isRead)
	{
		if(associativity==1)
		{
			sets[setIndex][0].tag = tag;
			if(!sets[setIndex][0].valid_bit)
			{
				validCnt[setIndex]++;
				totValidCnt++;
			}
			if(sets[setIndex][0].dirty_bit)
			{
				writeToNextLevelCache();
			}
			if(!isRead)
			{
				sets[setIndex][0].dirty_bit = true;
			}
			else
			{
				sets[setIndex][0].dirty_bit = false;
			}
			sets[setIndex][0].valid_bit = true;
			return;
		}
		int half = numBlocksInSet/2;
		int cur = numBlocksInSet - 2;//assigning root to cur;
		int child;
		while( cur >= half)
		{
            child = cur - half;
			child = child * 2 + lrutree[setIndex][cur];
			lrutree[setIndex][cur] = 1 - lrutree[setIndex][cur];//flipping
			cur = child;
		}
        child = 2*cur + lrutree[setIndex][cur];
		lrutree[setIndex][cur] = 1 - lrutree[setIndex][cur];//flipping
		sets[setIndex][child].tag = tag;
		if(!sets[setIndex][child].valid_bit)
		{
			validCnt[setIndex]++;
			totValidCnt++;
		}
        if(sets[setIndex][child].valid_bit && sets[setIndex][child].dirty_bit)
        {
            writeToNextLevelCache();
        }
		if(!isRead)
		{
			sets[setIndex][child].dirty_bit = true;
		}
		else
		{
			sets[setIndex][child].dirty_bit = false;
		}
		sets[setIndex][child].valid_bit = true;
	}
	/*
	Purpose:
	Read function to process a read acces to cache.
	Parameter(s):
	<1> tag 	: 	Tag of memory reference
	<2> setIndex:	setIndex of memory reference
	returns: N/A
	*/
	void read(int tag,int setIndex)
	{
		//check if tag is present,if present update recency
		bool isFound = false;
		int emptyBlock = -1;
		for(int i = 0;i < numBlocksInSet;i++)
		{
			if(sets[setIndex][i].valid_bit && sets[setIndex][i].tag == tag)
			{
				if(replacement_policy == 1)
				{
					int sz = recencyorder[setIndex].size();
					for(int j = sz-1;j >= 0;j--)
					{
						if(recencyorder[setIndex][j] == i)
						{
							for(int k = j+1;j < sz;j++)
							{
								recencyorder[setIndex][k-1] = recencyorder[setIndex][k];
							}
							recencyorder[setIndex][sz-1] = i;
							break;
						}
					}
				}
				else if(replacement_policy==2)
				{
					pseudoLRUHit(setIndex,i);
				}
				else if(replacement_policy == 0)
				{
					break;
				}
				else
				{
					assert(0);
				}
				isFound = true;
				break;
			}
			if(emptyBlock==-1 && !sets[setIndex][i].valid_bit)
			{
				emptyBlock = i;
			}
		}
		//if not present use replacement policies
		if(!isFound)
		{
			cache_misses++;
			read_misses++;
			if(totValidCnt == setCnt * numBlocksInSet)
			{
				capacity_misses++;
			}
			if(validCnt[setIndex]==numBlocksInSet && totValidCnt < setCnt * numBlocksInSet)
			{
				//check if it was present previously
				if(compulsoryMissFinder.find((tag << setIndLen) + setIndex)!=compulsoryMissFinder.end())
				{
					conflict_misses++;
				}
			}
			//get from next level cache then replace it in this cache
			if(emptyBlock!=-1)
			{
				if(replacement_policy == 1)
				{
					if(!sets[setIndex][emptyBlock].valid_bit)
					{
						validCnt[setIndex]++;
						totValidCnt++;
					}
					sets[setIndex][emptyBlock].valid_bit = true;
					sets[setIndex][emptyBlock].tag = tag;
					recencyorder[setIndex].push_back(emptyBlock);
				}
				else if(replacement_policy==2)
				{
					pseudoLRUreplace(setIndex,tag,true);
				}
				else if(replacement_policy == 0)
				{
					if(!sets[setIndex][emptyBlock].valid_bit)
					{
						validCnt[setIndex]++;
						totValidCnt++;
					}
					sets[setIndex][emptyBlock].valid_bit = true;
					sets[setIndex][emptyBlock].tag = tag;
				}
				else
				{
					assert(0);
				}
			}
			else
			{
				if(replacement_policy==1)
				{
					int sz = recencyorder[setIndex].size();
                    emptyBlock = recencyorder[setIndex][0];
					if(sets[setIndex][recencyorder[setIndex][0]].dirty_bit)
					{
						writeToNextLevelCache();
						sets[setIndex][recencyorder[setIndex][0]].dirty_bit = false;
					}
					for(int i = 1;i < sz ;i++)
					{
						recencyorder[setIndex][i-1] = recencyorder[setIndex][i];
					}
					sets[setIndex][emptyBlock].tag = tag;
					recencyorder[setIndex][sz-1] = emptyBlock;
				}
				else if(replacement_policy == 2)
				{
					pseudoLRUreplace(setIndex,tag,true);
				}
				else if(replacement_policy == 0)
				{
					int repIndex = rand() % numBlocksInSet;
					if(sets[setIndex][repIndex].dirty_bit)
					{
						writeToNextLevelCache();
						sets[setIndex][repIndex].dirty_bit = false;
					}
					sets[setIndex][repIndex].tag = tag;
				}
				else
				{
					assert(0);
				}
			}

		}
	}
	/*
	Purpose:
	Write function to process a write acces to cache.
	Parameter(s):
	<1> tag 	: 	Tag of memory reference
	<2> setIndex:	setIndex of memory reference
	returns: N/A
	*/
	void write(int tag,int setIndex)
	{
		//check if tag is present,if present update recency
		bool isFound = false;
		int emptyBlock = -1;
		for(int i = 0;i < numBlocksInSet;i++)
		{
			if(sets[setIndex][i].valid_bit && sets[setIndex][i].tag == tag)
			{
				if(replacement_policy == 1)
				{
					int sz = recencyorder.at(setIndex).size();
					for(int j = sz-1;j >= 0;j--)
					{
						if(recencyorder.at(setIndex).at(j) == i)
						{
							for(int k = j+1;j < sz;j++)
							{
								recencyorder[setIndex][k-1] = recencyorder[setIndex][k];
							}
							recencyorder[setIndex][sz-1] = i;
							break;
						}
					}
				}
				else if(replacement_policy==2)
				{
					pseudoLRUHit(setIndex,i);
				}
				else if(replacement_policy == 0)
				{
					break;
				}
				else
				{
					assert(0);
				}
				sets[setIndex][i].dirty_bit = true;
				isFound = true;
				break;
			}
			if(emptyBlock==-1 && !sets[setIndex][i].valid_bit)
			{
				emptyBlock = i;
			}
		}
		//if not present use replacement policies
		if(!isFound)
		{
			cache_misses++;
			write_misses++;
			if(totValidCnt == setCnt * numBlocksInSet)
			{
				capacity_misses++;
			}
			if(validCnt[setIndex]==numBlocksInSet && totValidCnt < setCnt * numBlocksInSet)
			{
				//check if it was present previously
				if(compulsoryMissFinder.find((tag << setIndLen) + setIndex)!=compulsoryMissFinder.end())
				{
					conflict_misses++;
				}
			}
			//get from next level cache then replace it in this cache
			if(emptyBlock!=-1)
			{
				if(replacement_policy == 1)
				{
					if(!sets[setIndex][emptyBlock].valid_bit)
					{
						validCnt[setIndex]++;
						totValidCnt++;
					}
					sets[setIndex][emptyBlock].valid_bit = true;
					sets[setIndex][emptyBlock].tag = tag;
					recencyorder[setIndex].push_back(emptyBlock);
				}
				else if(replacement_policy==2)
				{
					pseudoLRUreplace(setIndex,tag,false);
				}
				else if(replacement_policy == 0)
				{
					if(!sets[setIndex][emptyBlock].valid_bit)
					{
						validCnt[setIndex]++;
						totValidCnt++;
					}
					sets[setIndex][emptyBlock].valid_bit = true;
					sets[setIndex][emptyBlock].tag = tag;
				}
				else
				{
					assert(0);
				}
				sets[setIndex][emptyBlock].dirty_bit = true;
			}
			else
			{
				if(replacement_policy==1)
				{
					int sz = recencyorder[setIndex].size();
                    emptyBlock = recencyorder[setIndex][0];
					if(sets[setIndex][recencyorder.at(setIndex).at(0)].dirty_bit)
					{
						writeToNextLevelCache();
						sets[setIndex][recencyorder[setIndex][0]].dirty_bit = false;
					}
					for(int i = 1;i < sz ;i++)
					{
						recencyorder.at(setIndex).at(i-1) = recencyorder[setIndex][i];
					}
					recencyorder[setIndex][sz-1] = emptyBlock;
					sets[setIndex][emptyBlock].tag = tag;
					sets[setIndex][emptyBlock].dirty_bit = true;
				}
				else if(replacement_policy == 2)
				{
					pseudoLRUreplace(setIndex,tag,false);
				}
				else if(replacement_policy == 0)
				{
					int repIndex = rand() % numBlocksInSet;
					if(sets[setIndex][repIndex].dirty_bit)
					{
						writeToNextLevelCache();
						sets[setIndex][repIndex].dirty_bit = false;
					}
					sets[setIndex][repIndex].tag = tag;
					sets[setIndex][repIndex].dirty_bit = true;
				}
				else
				{
					assert(0);
				}
			}

		}
	}
	/*
	Purpose:
	decode function that takes a memory reference along with read or write signal and decodes address to tag and setIndex and calls appropriate functions
	Parameter(s):
	<1> hexAddress :string whose first word contains hex address and second word is a character r or w which tells if it is a read or write respectively
	returns: N/A
	*/
	void decode(string hexAddress)
	{
		cache_access++;
		stringstream X(hexAddress);
		string bin;
		X >> bin;
		string isread;
		X >> isread;
		int address = stoi(bin,0,16);
		int blockID = (address >> blockOffsetLen);
		bool isRead = (isread[0] == 'r');
		int tag = (address >> (blockOffsetLen + setIndLen));
		int setIndex = address - (tag << (blockOffsetLen + setIndLen));
		setIndex = (setIndex >> blockOffsetLen);
		if(isRead)
		{
			read(tag,setIndex);
			read_access++;
		}
		else
		{
			write(tag,setIndex);
			write_access++;
		}
		if(compulsoryMissFinder.find(blockID)==compulsoryMissFinder.end())
		{
			compulsory_misses++;
			compulsoryMissFinder.insert(blockID);
		}
	}

};
int main()
{
	int cache_size;																//Take appropriate variables from stdin and process requests
	cin>>cache_size;
	int block_size;
	cin>>block_size;
	int associativity;
	cin>>associativity;
	int replacement_policy;
	cin >> replacement_policy;
	cache C(cache_size,block_size,associativity,replacement_policy);
	string address;
	cin >> address;
	string s;
	ifstream myfile (address);
	while(getline(myfile,s))
	{
	    if(s.size() > 0)
		C.decode(s);
	}
	myfile.close();
  	cout <<cache_size<<"\n";
  	cout <<block_size<<"\n";

  	if(associativity==0) cout <<"fully-associative\n";
  	else if(associativity==1) cout <<"direct-mapped\n";
  	else cout <<"set-associative\n";

  	if(replacement_policy==0) cout <<"random\n";
  	else if(replacement_policy==1) cout <<"LRU\n";
  	else cout <<"Pseudo-LRU\n";

  	cout <<cache_access<<"\n";
  	cout <<read_access<<"\n";
  	cout <<write_access<<"\n";
  	cout <<cache_misses<<"\n";
  	cout <<compulsory_misses<<"\n";
  	cout <<capacity_misses<<"\n";
  	cout <<conflict_misses<<"\n";
  	cout <<read_misses<<"\n";
  	cout <<write_misses<<"\n";
  	cout <<dirty_blocks_evicted<<"\n";

}

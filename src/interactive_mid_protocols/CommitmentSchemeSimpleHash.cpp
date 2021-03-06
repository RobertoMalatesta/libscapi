/**
* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
* 
* Copyright (c) 2016 LIBSCAPI (http://crypto.biu.ac.il/SCAPI)
* This file is part of the SCAPI project.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
* and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
* FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
* We request that any publication and/or code referring to and/or based on SCAPI contain an appropriate citation to SCAPI, including a reference to
* http://crypto.biu.ac.il/SCAPI.
* 
* Libscapi uses several open source libraries. Please see these projects for any further licensing issues.
* For more information , See https://github.com/cryptobiu/libscapi/blob/master/LICENSE.MD
*
* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
* 
*/


#include "../../include/interactive_mid_protocols/CommitmentSchemeSimpleHash.hpp"


/****************************************/
/*********** Commitment message *********/
/****************************************/
void CmtSimpleHashCommitmentMessage::initFromString(const string & s) {
	auto vec = explode(s, ':');
	id = stol(vec[0]);
	for (int i = 2; i < vec.size(); i++) {
		vec[1] += ":" + vec[i];
	}
	c = make_shared<vector<byte>>(vec[1].begin(), vec[1].end());
}

string CmtSimpleHashCommitmentMessage::toString() {
	string output = to_string(id);
	output += ":";
	const byte * uc = &((*c)[0]);
	output += string(reinterpret_cast<char const*>(uc), c->size());
	return output;
};

/****************************************/
/*********** Decommitment message *********/
/****************************************/
void CmtSimpleHashDecommitmentMessage::initFromString(const string & s) {
	auto vec = explode(s, ':');
	auto size = stoull(vec[0]);
	int counter = 2;
	while (vec[1].size() != size)
		vec[1] += ":" + vec[counter++];
	vector<byte> random(vec[1].begin(), vec[1].end());
	r = make_shared<ByteArrayRandomValue>(random);
	for (int i = counter + 1; i < vec.size(); i++)
		vec[counter] += ":" + vec[i];
	vector<byte> tmp(vec[counter].begin(), vec[counter].end());
	x = make_shared<vector<byte>>(tmp);
}

string CmtSimpleHashDecommitmentMessage::toString() {
	auto random = r->getR();
	auto size = random.size();
	string output = to_string(size);
	output += ":";
	const byte * uc = &(random[0]);
	output += string(reinterpret_cast<char const*>(uc), random.size());
	const byte * xBytes = &((*x)[0]);
	output += ":";
	output += string(reinterpret_cast<char const*>(xBytes), x->size());
	return output;
};

/**
* Constructor that receives a connected channel (to the receiver) and chosses default
* values for the hash function, SecureRandom object and a security parameter n.
*  @param channel
*/
CmtSimpleHashCommitter::CmtSimpleHashCommitter(shared_ptr<CommParty> channel, shared_ptr<CryptographicHash> hash, int n) {
	auto prg = make_shared<PrgFromOpenSSLAES>();
	prg->setKey(prg->generateKey(128));
	
	init(channel, prg, hash, n);
}

/**
* Constructor that receives a connected channel (to the receiver) and chosses default
* values for the hash function, SecureRandom object and a security parameter n.
*  @param channel
*/
CmtSimpleHashCommitter::CmtSimpleHashCommitter(shared_ptr<CommParty> channel, shared_ptr<PrgFromOpenSSLAES> prg, shared_ptr<CryptographicHash> hash, int n) {
	init(channel, prg, hash, n);
}

void CmtSimpleHashCommitter::init(shared_ptr<CommParty> channel, shared_ptr<PrgFromOpenSSLAES> prg, shared_ptr<CryptographicHash> hash, int n) {
	this->channel = channel;
	this->hash = hash;
	this->n = n;
	this->prg = prg;

	//No pre-process in SimpleHash Commitment
}

/**
* Runs the following lines of the commitment scheme:
* "SAMPLE a random value r <- {0, 1}^n
*	COMPUTE c = H(r,x) (c concatenated with r)".
* @return the generated commitment.
*
*/
shared_ptr<CmtCCommitmentMsg> CmtSimpleHashCommitter::generateCommitmentMsg(shared_ptr<CmtCommitValue> input, long id) {
	auto in = dynamic_pointer_cast<CmtByteArrayCommitValue>(input);
	if (in == NULL)
		throw invalid_argument("The input has to be of type CmtByteArrayCommitValue");
	auto x = in->getXVector();
	//Sample random byte array r
	vector<byte> r(n);
	prg->getPRGBytes(r, 0, n);

	//Compute the hash function
	auto hashValArray = computeCommitment(*x, r);

	//After succeeding in sending the commitment, keep the committed value in the map together with its ID.
	commitmentMap[id] = make_shared<CmtSimpleHashCommitmentValues>(make_shared<ByteArrayRandomValue>(r), input, hashValArray);

	return make_shared<CmtSimpleHashCommitmentMessage>(hashValArray, id);
}

shared_ptr<CmtCDecommitmentMessage> CmtSimpleHashCommitter::generateDecommitmentMsg(long id)  {
	//fetch the commitment according to the requested ID
	auto vals = commitmentMap[id];
	auto x = static_pointer_cast<vector<byte>>(vals->getX()->getX());
	auto r = static_pointer_cast<ByteArrayRandomValue>(commitmentMap[id]->getR());
	return make_shared<CmtSimpleHashDecommitmentMessage>(r, x);
}

/**
* This function samples random commit value and returns it.
* @return the sampled commit value
*/
shared_ptr<CmtCommitValue> CmtSimpleHashCommitter::sampleRandomCommitValue() {
	vector<byte> val;
	//RAND_bytes(val.data(), 32);
	prg->getPRGBytes(val, 0, 32);

	return make_shared<CmtByteArrayCommitValue>(make_shared<vector<byte>>(val));
}

/**
* This function converts the given commit value to a byte array.
* @param value
* @return the generated bytes.
*/
vector<byte> CmtSimpleHashCommitter::generateBytesFromCommitValue(CmtCommitValue* value) {
	auto val = dynamic_cast<CmtByteArrayCommitValue*>(value);
	if (val == NULL)
		throw invalid_argument("The given value must be of type CmtByteArrayCommitValue");
	return *val->getXVector();
}

/**
* Computes the hash function on the concatination of the inputs.
* @param x user input
* @param r random value
* @return the hash result.
*/
shared_ptr<vector<byte>> CmtSimpleHashCommitter::computeCommitment(vector<byte> x, vector<byte> r) {
	//create an array that will hold the concatenation of r with x
	vector<byte> c(r);
	c.insert(c.end(), x.begin(), x.end());

	auto hashValArray = make_shared<vector<byte>>(hash->getHashedMsgSize());
	hash->update(c, 0, c.size());
	hash->hashFinal(*hashValArray, 0);
	return hashValArray;
}

void CmtSimpleHashReceiver::doConstruct(shared_ptr<CommParty> channel, shared_ptr<CryptographicHash> hash, int n) {
	this->channel = channel;
	this->hash = hash;
	this->n = n;
	
	//No pre-process in SimpleHash Commitment
}

/**
* Run the commit phase of the protocol:
* "WAIT for a value c
*	STORE c".
*/
shared_ptr<CmtRCommitPhaseOutput> CmtSimpleHashReceiver::receiveCommitment() {

	// create an empty CmtPedersenCommitmentMessage 
	auto msg = make_shared<CmtSimpleHashCommitmentMessage>();

	// read encoded CmtPedersenCommitmentMessage from channel
	vector<byte> raw_msg; // by the end of the scope - no need to hold it anymore - already decoded and copied
	channel->readWithSizeIntoVector(raw_msg);
	// init the empy CmtPedersenCommitmentMessage using the encdoed data
	msg->initFromByteVector(raw_msg);

	commitmentMap[msg->getId()] = msg;
	return make_shared<CmtRBasicCommitPhaseOutput>(msg->getId());
}

/**
* Run the decommit phase of the protocol:
* "WAIT for (r, x)  from C
*	IF NOT
*		c = H(r,x), AND
*		x <- {0, 1}^t
*		OUTPUT REJ
*	ELSE
*	  	OUTPUT ACC and value x".
*/
shared_ptr<CmtCommitValue> CmtSimpleHashReceiver::receiveDecommitment(long id) {
	//Receive the message from the committer.
	vector<byte> raw_msg;
	channel->readWithSizeIntoVector(raw_msg);
	auto msg = make_shared<CmtSimpleHashDecommitmentMessage>();
	msg->initFromByteVector(raw_msg);
	auto receivedCommitment = commitmentMap[id];
	auto cmtCommitMsg = static_pointer_cast<CmtCCommitmentMsg>(receivedCommitment);
	return verifyDecommitment(cmtCommitMsg.get(), msg.get());
}

shared_ptr<CmtCommitValue> CmtSimpleHashReceiver::verifyDecommitment(CmtCCommitmentMsg* commitmentMsg, CmtCDecommitmentMessage* decommitmentMsg) {
	auto decomMsg = dynamic_cast<CmtSimpleHashDecommitmentMessage*>(decommitmentMsg);
	if (decomMsg == NULL) {
		throw invalid_argument("the received message is not an instance of CmtSimpleHashDecommitmentMessage");
	}

	auto comMsg = dynamic_cast<CmtSimpleHashCommitmentMessage*>(commitmentMsg);
	if (comMsg == NULL) {
		throw invalid_argument("the received message is not an instance of CmtSimpleHashCommitmentMessage");
	}
	
	//Compute c = H(r,x)
	auto x = decomMsg->getXValue();
	auto r = dynamic_pointer_cast<ByteArrayRandomValue>(decomMsg->getR())->getR();
	
	//create an array that will hold the concatenation of r with x
	vector<byte> cTag(r);
	cTag.insert(cTag.end(), x.begin(), x.end());	
	vector<byte> hashValArrayTag;
	hash->update(cTag, 0, cTag.size());
	hash->hashFinal(hashValArrayTag, 0);

	//Checks that c = H(r,x)
	auto commitment = *static_pointer_cast<vector<byte>>(comMsg->getCommitment());
	if (commitment == hashValArrayTag)
		return make_shared<CmtByteArrayCommitValue>(make_shared<vector<byte>>(x));
	//In the pseudocode it says to return X and ACCEPT if valid commitment else, REJECT.
	//For now we return null as a mode of reject. If the returned value of this function is not null then it means ACCEPT
	return NULL;
}

/**
* This function converts the given commit value to a byte array.
* @param value
* @return the generated bytes.
*/
vector<byte> CmtSimpleHashReceiver::generateBytesFromCommitValue(CmtCommitValue* value)  {
	auto val = dynamic_cast<CmtByteArrayCommitValue*>(value);
	if (val == NULL)
		throw invalid_argument("The given value must be of type CmtByteArrayCommitValue");
	return *val->getXVector();
}
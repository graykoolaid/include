// This code contains NVIDIA Confidential Information and is disclosed to you 
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and 
// any modifications thereto. Any use, reproduction, disclosure, or 
// distribution of this software and related documentation without an express 
// license agreement from NVIDIA Corporation is strictly prohibited.
// 
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2012 NVIDIA Corporation. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.  


#ifndef PX_PHYSX_PROFILE_DATA_BUFFER_H
#define PX_PHYSX_PROFILE_DATA_BUFFER_H
#include "PxProfileFoundationWrapper.h"
#include "PxProfileMemoryBuffer.h"
#include "PxProfileEventBufferClient.h"

namespace physx { namespace profile {

	template<typename TMutex
			, typename TScopedLock>
	class DataBuffer //base class for buffers that cache data and then dump the data to clients.
	{
	public:
		typedef TMutex				TMutexType;
		typedef TScopedLock			TScopedLockType;
		typedef WrapperNamedAllocator TU8AllocatorType;

		typedef MemoryBuffer<TU8AllocatorType > TMemoryBufferType;
		typedef ProfileArray<PxProfileEventBufferClient*> TBufferClientArray;

	protected:
		
		FoundationWrapper					mWrapper;
		TMemoryBufferType					mDataArray;
		TBufferClientArray					mClients;
		PxU32								mBufferFullAmount;
		EventContextInformation				mEventContextInformation;
		PxU64								mLastTimestamp;
		TMutexType*							mBufferMutex;
		volatile bool						mHasClients;
		EventSerializer<TMemoryBufferType >	mSerializer;

	public:
		
		DataBuffer( PxAllocatorCallback* inFoundation
					, PxU32 inBufferFullAmount
					, TMutexType* inBufferMutex
					, const char* inAllocationName )
			: mWrapper( inFoundation )
			, mDataArray( TU8AllocatorType( mWrapper, inAllocationName ) )
			, mClients( mWrapper )
			, mBufferFullAmount( inBufferFullAmount )
			, mBufferMutex( inBufferMutex )
			, mHasClients( false )
			, mSerializer( &mDataArray )
		{
			//The data array is never resized really.  We ensure
			//it is bigger than it will ever need to be.
			mDataArray.reserve( inBufferFullAmount + 68 );
		}
		
		virtual ~DataBuffer()
		{
			while( mClients.size() )
			{
				removeClient( *mClients[0] );
			}
		}

		FoundationWrapper& getWrapper() { return mWrapper; }
		TMutexType*		  getBufferMutex() { return mBufferMutex; }
		void			  setBufferMutex(TMutexType* mutex) { mBufferMutex = mutex; }

		void addClient( PxProfileEventBufferClient& inClient ) 
		{ 
			TScopedLockType lock( mBufferMutex ); 
			mClients.pushBack( &inClient );
			mHasClients = true;
		}

		void removeClient( PxProfileEventBufferClient& inClient ) 
		{
			TScopedLockType lock( mBufferMutex );
			for ( PxU32 idx =0; idx < mClients.size(); ++idx )
			{
				if ( mClients[idx] == &inClient )
				{
					inClient.handleClientRemoved();
					mClients.replaceWithLast( idx );
					break;
				}
			}
			mHasClients = mClients.size() != 0;
		}

		
		bool hasClients() const 
		{ 
			return mHasClients;
		}

		virtual void flushEvents()
		{	
			TScopedLockType lock( mBufferMutex );
			const PxU8* theData = mDataArray.begin();
			PxU32 theDataSize = mDataArray.size();
			sendDataToClients( theData, theDataSize );
			mDataArray.clear();
			clearCachedData();
		}

		//Used for chaining together event buffers.
		virtual void handleBufferFlush( const PxU8* inData, PxU32 inDataSize )
		{
			TScopedLockType lock( mBufferMutex );
			if ( inData && inDataSize )
			{
				clearCachedData();
				if ( mDataArray.size() + inDataSize >= mBufferFullAmount )
					flushEvents();
				if ( inDataSize >= mBufferFullAmount )
					sendDataToClients( inData, inDataSize );
				else
					mDataArray.write( inData, inDataSize );
			}
		}

	protected:
		virtual void clearCachedData()
		{
		}

	private:
			
		void sendDataToClients( const PxU8* inData, PxU32 inDataSize )
		{
			PxU32 clientCount = mClients.size();
			for( PxU32 idx =0; idx < clientCount; ++idx )
				mClients[idx]->handleBufferFlush( inData, inDataSize );
		}

	};

}}


#endif
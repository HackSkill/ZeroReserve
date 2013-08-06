/*
    This file is part of the Zero Reserve Plugin for Retroshare.

    Zero Reserve is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Zero Reserve is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with Zero Reserve.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "TransactionManager.h"
#include "RSZeroReserveItems.h"
#include "ZeroReservePlugin.h"
#include "p3ZeroReserverRS.h"
#include "zrdb.h"
#include "Payment.h"

#include <stdexcept>
#include <sstream>



TransactionManager::TxManagers TransactionManager::currentTX;


bool TransactionManager::handleTxItem( RsZeroReserveTxItem * item )
{
    std::cerr << "Zero Reserve: TX Manger handling incoming item" << std::endl;
    std::string id = item->PeerId();
    TxManagers::iterator it = currentTX.find( id );
    if( it == currentTX.end() ){
        if( item->getTxPhase() == QUERY ){  // a new request to receive or forward payment
            RsZeroReserveInitTxItem * initItem = dynamic_cast<RsZeroReserveInitTxItem *>( item );
            if(!initItem) return false;

            TransactionManager *tm = new TransactionManager( );
            currentTX[ id ] = tm;
            try {
                return tm->initCohort( initItem );
            }
            catch( std::runtime_error e ){
                std::cerr << "Zero Reserve: TX Manger: Error: " << e.what() << std::endl;
                currentTX.erase( id );
                delete tm;
                return false;
            }
        }
        else {
            std::cerr << "Zero Reserve: TX Manger: Error: Received first TX item but is not phase QUERY" << std::endl;
            return false;
        }
    }
    TransactionManager *tm = (*it).second;
    bool end_tx = true;
    try{
        end_tx = tm->processItem( item );
    }
    catch( std::runtime_error e ){
        std::cerr << "Zero Reserve: TX Manger: Error: " << e.what() << std::endl;
        currentTX.erase( id );
        delete tm;
        return false;
    }

// TODO:   delete item;
    if( end_tx ){
        currentTX.erase( id );
        delete tm;
    }
    return true;
}



TransactionManager::TransactionManager()
{
    // TODO: QTimer
}

TransactionManager::~TransactionManager()
{
    std::cerr << "Zero Reserve: TX Manager: Cleaning up." << std::endl;
    delete m_credit;
    delete m_payment;
}

bool TransactionManager::initCohort( RsZeroReserveInitTxItem * item )
{
    m_payment = item->getPayment();
    std::cerr << "Zero Reserve: TX Manager: Payment request for " << m_payment->getAmount() << " "
              << m_payment->getCurrency()
              << " received - Setting TX manager up as cohorte" << std::endl;
    m_credit = new Credit( item->PeerId(), m_payment->getCurrency() );
    m_credit->loadPeer();
    // TODO: multi hop
// TODO    m_role = (Role)item->getRole();
    m_role = Payee; // FIXME
    if ( atoi( m_credit->m_credit.c_str()) < m_payment->newBalance( m_credit ) ){
        std::cerr << "Zero Reserve: initCohort(): Insufficient Credit" << std::endl;
    }
    RsZeroReserveTxItem * reply = new RsZeroReserveTxItem( VOTE_YES );  // TODO: VOTE_NO
    reply->PeerId( m_credit->m_id );
    p3ZeroReserveRS * p3zs = static_cast< p3ZeroReserveRS* >( g_ZeroReservePlugin->rs_pqi_service() );
    p3zs->sendItem( reply ); // TODO: error  handling
    return true;
}

bool TransactionManager::initCoordinator( Payment * payment )
{
    std::cerr << "Zero Reserve: Setting TX manager up as coordinator" << std::endl;
    m_payment = payment;
    m_role = Coordinator;
    currentTX[ m_payment->getCounterparty() ] = this;
    m_credit = new Credit( m_payment->getCounterparty(), m_payment->getCurrency() );
    m_credit->loadPeer();
    if ( atoi( m_credit->m_our_credit.c_str()) - m_payment->newBalance( m_credit ) < 0 ){
        std::cerr << "Zero Reserve: Error, not enough Credit " << std::endl;
        return false;
    }
    RsZeroReserveInitTxItem * initItem = new RsZeroReserveInitTxItem( m_payment );
    p3ZeroReserveRS * p3zr = static_cast< p3ZeroReserveRS* >( g_ZeroReservePlugin->rs_pqi_service() );
    p3zr->sendItem( initItem );
    return true;
}

bool TransactionManager::processItem( RsZeroReserveTxItem * item )
{
    RsZeroReserveTxItem * reply;
    p3ZeroReserveRS * p3zs;

    // TODO: Timeout
    switch( item->getTxPhase() )
    {
    case QUERY:
        abortTx( item ); // we should never get here
        throw std::runtime_error( "Dit not expect QUERY" );
    case VOTE_YES:
        if( m_role != Coordinator ) throw std::runtime_error( "Dit not expect VOTE (YES)");
        std::cerr << "Zero Reserve: TX Coordinator: Received Vote: YES" << std::endl;
        reply = new RsZeroReserveTxItem( COMMIT );
        reply->PeerId( m_credit->m_id );
        p3zs = static_cast< p3ZeroReserveRS* >( g_ZeroReservePlugin->rs_pqi_service() );
        p3zs->sendItem( reply );
        return false;
    case VOTE_NO:
        break;
    case COMMIT:
        if( m_role != Payee ) throw std::runtime_error( "Dit not expect COMMIT" ); // TODO: Hop
        std::cerr << "Zero Reserve: TX Cohorte: Received Command: COMMIT" << std::endl;
        reply = new RsZeroReserveTxItem( ACK_COMMIT );
        reply->PeerId( m_credit->m_id );
        p3zs = static_cast< p3ZeroReserveRS* >( g_ZeroReservePlugin->rs_pqi_service() );
        p3zs->sendItem( reply );
        commit();
        return true;
    case ACK_COMMIT:
        if( m_role != Coordinator ) throw std::runtime_error( "Dit not expect ACK_COMMIT");
        std::cerr << "Zero Reserve: TX Coordinator: Received Acknowledgement, Committing" << std::endl;
        commit();
        return true;
    case ABORT:
        abortTx( item );
        return true;
    default:
        throw std::runtime_error( "Unknown Transaction Phase");
    }
    return true;
}

void TransactionManager::abortTx( RsZeroReserveTxItem * item )
{
     std::cerr << "Zero Reserve: TX Manger:Error happened. Aborting." << std::endl;
}


void TransactionManager::commit()
{
    std::ostringstream balance;
    balance << m_payment->newBalance( m_credit );
    m_credit->m_balance = balance.str();

    ZrDB::Instance()->updatePeerCredit( *m_credit, "balance", m_credit->m_balance );
}




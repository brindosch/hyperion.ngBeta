// system includes
#include <stdexcept>

// project includes
#include <boblightserver/BoblightServer.h>
#include "BoblightClientConnection.h"

// hyperion includes
#include <hyperion/Hyperion.h>
#include <bonjour/bonjourserviceregister.h>

using namespace hyperion;

BoblightServer::BoblightServer(const QJsonObject& config)
	: QObject()
	, _hyperion(Hyperion::getInstance())
	, _server()
	, _openConnections()
	, _priority(0)
	, _log(Logger::getInstance("BOBLIGHT"))
	, _isActive(false)
	, _port(0)
{
	Debug(_log, "Instance created");

	// init
	handleSettingsUpdate(config);
	// Set trigger for incoming connections
	connect(&_server, SIGNAL(newConnection()), this, SLOT(newConnection()));
}

BoblightServer::~BoblightServer()
{
	stop();
}

void BoblightServer::start()
{
	if ( active() )
		return;

	if (!_server.listen(QHostAddress::Any, _port))
	{
		throw std::runtime_error("BOBLIGHT ERROR: server could not bind to port");
	}
	Info(_log, "Boblight server started on port %d", _port);

	_isActive = true;

	_hyperion->registerPriority("Boblight", _priority);
	_hyperion->getComponentRegister().componentStateChanged(COMP_BOBLIGHTSERVER, _isActive);

	if(_bonjourService == nullptr)
	{
		_bonjourService = new BonjourServiceRegister();
		_bonjourService->registerService("_hyperiond-bobl._tcp", _port);
	}
}

void BoblightServer::stop()
{
	if ( ! active() )
		return;

	foreach (BoblightClientConnection * connection, _openConnections) {
		delete connection;
	}
	_server.close();
	_isActive = false;

	_hyperion->unRegisterPriority("Boblight");
	_hyperion->getComponentRegister().componentStateChanged(COMP_BOBLIGHTSERVER, _isActive);
}

void BoblightServer::componentStateChanged(const hyperion::Components component, bool enable)
{
	if (component == COMP_BOBLIGHTSERVER)
	{
		if (_isActive != enable)
		{
			if (enable) start();
			else        stop();
			Info(_log, "change state to %s", (_isActive ? "enabled" : "disabled") );
		}
	}
}

uint16_t BoblightServer::getPort() const
{
	return _server.serverPort();
}

void BoblightServer::newConnection()
{
	QTcpSocket * socket = _server.nextPendingConnection();

	if (socket != nullptr)
	{
		Info(_log, "new connection");
		BoblightClientConnection * connection = new BoblightClientConnection(socket, _priority);
		_openConnections.insert(connection);

		// register slot for cleaning up after the connection closed
		connect(connection, SIGNAL(connectionClosed(BoblightClientConnection*)), this, SLOT(closedConnection(BoblightClientConnection*)));
	}
}

void BoblightServer::closedConnection(BoblightClientConnection *connection)
{
	Debug(_log, "connection closed");
	_openConnections.remove(connection);

	// schedule to delete the connection object
	connection->deleteLater();
}

void BoblightServer::handleSettingsUpdate(const QJsonObject& obj)
{
	_port = obj["port"].toInt();
	_priority = obj["priority"].toInt();
	stop();
	if(obj["enable"].toBool())
		start();
}

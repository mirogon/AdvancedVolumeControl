#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    ui->setupUi(this);
    this->setWindowTitle("Advanced Volume Control");
    this->setFixedSize(size());
    this->statusBar()->setSizeGripEnabled(false);

    parentWidget = new QWidget();
    parentLayout = new QGridLayout();
    parentLayout->setContentsMargins(5, 5, 5, 5);

    parentWidget->setLayout(parentLayout);

    this->setCentralWidget(parentWidget);

    itemLayout = new QGridLayout;
    itemLayout->setHorizontalSpacing(0);
    itemLayout->setContentsMargins(0,0,0,0);
    itemLayout->setMargin(0);

    scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scrollArea->setFixedWidth(this->width() * 0.95);
    scrollArea->setFixedHeight(this->height() * 0.8);

    scrollAreaContent = new QWidget();
    scrollAreaContent->setLayout(itemLayout);
    scrollAreaContent->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

    mapper = new QSignalMapper(this);
    mapper2 = new QSignalMapper(this);
    mapper3 = new QSignalMapper(this);

    connect(mapper, SIGNAL(mapped(int)), this, SLOT(SliderMoved(int)));
    connect(mapper2, SIGNAL(mapped(int)), this, SLOT(SpinBoxChanged(int)));
    connect(mapper3, SIGNAL(mapped(int)), this, SLOT(SliderReleased(int)));

    socket = nullptr;
    server = new QTcpServer(this);

    if(server->listen(QHostAddress::Any, 55555) == false)
    {
        qDebug()<<"Server could not be initialized";
    }

    connect(server, SIGNAL(newConnection()), this, SLOT(NewClientConnection()));

    timer_receive = new QTimer(this);
    connect(timer_receive, SIGNAL(timeout()), this, SLOT(ReceivedClientPackage()));

    labelLocalIP = new QLabel;
    labelIsConnected = new QLabel;
    labelIsConnected->setText("Connected to mobile client: no");

    QString ipAddress;
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();

     // use the first non-localhost IPv4 address
     for (int i = 0; i < ipAddressesList.size(); ++i)
     {
        if (ipAddressesList.at(i) != QHostAddress::LocalHost && ipAddressesList.at(i).toIPv4Address())
        {
            ipAddress = ipAddressesList.at(i).toString();
            break;
        }
    }
     // if we did not find one, use IPv4 localhost
    if (ipAddress.isEmpty())
    {
        ipAddress = QHostAddress(QHostAddress::LocalHost).toString();
    }

    labelLocalIP->setText("Address: " + ipAddress);

    audioController.Initialize();

    audioController.GetAudioSessions(audioSessions);

    int rowAlign = 0;

    for(int i = 0; i < audioSessions.size(); ++i)
    {
        labels.push_back(new QLabel());
        labels.back()->setFixedWidth(this->width() * 0.7);
        labels.back()->setFixedHeight(10);
        slider.push_back(new QSlider());
        slider.back()->setFixedWidth(this->width() * 0.6);
        slider.back()->setFixedHeight(25);
        spinBoxes.push_back(new QSpinBox());
        spinBoxes.back()->setFixedWidth(this->width() * 0.2);
        spinBoxes.back()->setFixedHeight(25);

        slider.at(i)->setOrientation(Qt::Horizontal);
        labels.at(i)->setFont(QFont("Arial", 10, QFont::Bold));

        spinBoxes.at(i)->setMinimum(0);
        spinBoxes.at(i)->setMaximum(100);
        spinBoxes.at(i)->setValue(100 * audioSessions.at(i).currentVolumeLevel);

        slider.at(i)->setMinimum(0);
        slider.at(i)->setMaximum(100);
        slider.at(i)->setValue(100 * audioSessions.at(i).currentVolumeLevel);

        labels.at(i)->setText(audioSessions.at(i).displayName.c_str());
        if(labels.at(i)->text().size() == 0)
        {
            labels.at(i)->setText("No name could be found");
        }

        connect(slider.at(i), SIGNAL(valueChanged(int)), mapper, SLOT(map()) );
        connect(spinBoxes.at(i), SIGNAL(editingFinished()), mapper2, SLOT(map()) );
        connect(slider.at(i), SIGNAL(sliderReleased()), mapper3, SLOT(map()));

        mapper->setMapping(slider.at(i), i);
        mapper2->setMapping(spinBoxes.at(i), i);
        mapper3->setMapping(slider.at(i), i);

        itemLayout->addWidget(labels.at(i), rowAlign++ , 0);
        itemLayout->addWidget(slider.at(i), rowAlign, 0);
        itemLayout->addWidget(spinBoxes.at(i), rowAlign++, 1);

    }

    scrollArea->setWidget(scrollAreaContent);
    parentLayout->addWidget(scrollArea, 0, 0, 1, 2, Qt::AlignCenter);
    parentLayout->addWidget(labelLocalIP, 1, 0, Qt::AlignLeft | Qt::AlignBottom);
    parentLayout->addWidget(labelIsConnected, 1, 1, Qt::AlignRight | Qt::AlignBottom);

    updateTimer = new QTimer(this);
    connect( updateTimer, SIGNAL(timeout()), this, SLOT( Update() ) );
    updateTimer->start(100);

}

void MainWindow::UpdateSessions()
{
    std::vector<S_AudioSession> audioSessions_New;
    audioController.UpdateAudioSessions();
    audioController.GetAudioSessions(audioSessions_New);

    if(audioSessions_New.size() != audioSessions.size())
    {
        //std::cout<<"Update sessions"<<std::endl;

        audioSessions.clear();
        audioSessions.shrink_to_fit();
        audioSessions = audioSessions_New;

        InitializeUI();
        if(socket != nullptr)
        {
            if(socket->state() == QAbstractSocket::ConnectedState)
            {
                static uint8_t packetType = pT_resetAudioSessions;
                static char buffer[1];

                memcpy(buffer, &packetType, 1);
                socket->write(buffer, 1);
                SendAudioSessions();
            }
        }

    }

   for(int i = 0 ; i < audioSessions.size(); ++i)
   {
       if(audioSessions.at(i).displayName != audioSessions_New.at(i).displayName)
       {
           audioSessions.clear();
           audioSessions.shrink_to_fit();
           audioSessions = audioSessions_New;

           InitializeUI();
           if(socket != nullptr)
           {
               if(socket->state() == QAbstractSocket::ConnectedState)
               {
                   static uint8_t packetType = pT_resetAudioSessions;
                   static char buffer[1];

                   memcpy(buffer, &packetType, 1);
                   socket->write(buffer, 1);
                   SendAudioSessions();
                   break;
               }
           }
       }
   }

}



void MainWindow::SliderMoved(int index)
{

    audioController.SetVolume(index, float(slider.at(index)->value()) / float(100));

    if(slider.at(index)->value() != spinBoxes.at(index)->value())
    {
        spinBoxes.at(index)->setValue(slider.at(index)->value());
    }

    audioSessions.at(index).currentVolumeLevel = float(slider.at(index)->value()) / 100.0f;
}

void MainWindow::SliderReleased(int index)
{
    SendAudioSession(index);
}

void MainWindow::SpinBoxChanged(int index)
{

    audioController.SetVolume(index, float(slider.at(index)->value()) / float(100));

    if(slider.at(index)->value() != spinBoxes.at(index)->value())
    {
        slider.at(index)->setValue(spinBoxes.at(index)->value());
    }

    audioSessions.at(index).currentVolumeLevel = float(spinBoxes.at(index)->value()) / 100.0f;

    SendAudioSession(index);

}

void MainWindow::NewClientConnection()
{
    socket = server->nextPendingConnection();
    connect(socket, SIGNAL(disconnected()), this, SLOT(ClientDisconnected()));

    SendAudioSessions();

    labelIsConnected->setText("Connection to mobile client: yes");
    qDebug()<<"Client connected";

    timer_receive->start(100);

}

void MainWindow::ClientDisconnected()
{
    labelIsConnected->setText("Connection to mobile client: no");
}

void MainWindow::ReceivedClientPackage()
{

    if(socket != nullptr)
    {
        if(socket->state() == QAbstractSocket::ConnectedState )
        {
            if(socket->bytesAvailable() >= sizeof(S_AudioSession_Network_Receive))
            {
                qDebug()<<"Received client package";
                static S_AudioSession_Network_Receive receivedSession;
                static char buffer[sizeof(S_AudioSession_Network_Receive)];

                socket->read(buffer, sizeof(S_AudioSession_Network_Receive));

                memcpy(&receivedSession, buffer, sizeof(S_AudioSession_Network_Receive));

                audioSessions.at(receivedSession.index).currentVolumeLevel = float(receivedSession.currentVolumeLevel) * float(0.01);
                audioSessions.at(receivedSession.index).isMuted = receivedSession.isMuted;

                audioController.SetVolume(receivedSession.index, audioSessions.at(receivedSession.index).currentVolumeLevel);

                slider.at(receivedSession.index)->setValue(receivedSession.currentVolumeLevel);
                spinBoxes.at(receivedSession.index)->setValue(receivedSession.currentVolumeLevel);

            }
        }

    }

    timer_receive->start(100);

}

MainWindow::~MainWindow()
{
    SAFE_DELETE(itemLayout);
    SAFE_DELETE(updateTimer);
    SAFE_DELETE(mapper);
    SAFE_DELETE(mapper2);
    SAFE_DELETE(mapper3);

    SAFE_DELETE(timer_receive);
    SAFE_DELETE(socket);
    SAFE_DELETE(server);

    SAFE_DELETE(labelIsConnected);
    SAFE_DELETE(labelLocalIP);

    audioSessions.clear();

    for(int i = 0; i < slider.size();++i)
    {
        SAFE_DELETE(slider.at(i));
    }
    slider.clear();

    for(int i = 0; i < labels.size();++i)
    {
        SAFE_DELETE(labels.at(i));
    }
    labels.clear();

    for(int i = 0; i < spinBoxes.size();++i)
    {
        SAFE_DELETE(spinBoxes.at(i));
    }
    spinBoxes.clear();

    SAFE_DELETE(ui);
}

void MainWindow::InitializeUI()
{
     for(int i = 0; i < labels.size(); ++i)
     {
         SAFE_DELETE(labels.at(i));
     }
     labels.clear();
     labels.shrink_to_fit();

     for(int i = 0; i < slider.size(); ++i)
     {
         disconnect(slider.at(i), 0, 0, 0);
         mapper->removeMappings(slider.at(i));
         mapper2->removeMappings(slider.at(i));
         SAFE_DELETE(slider.at(i));
     }
     slider.clear();
     slider.shrink_to_fit();

     for(int i = 0; i < spinBoxes.size(); ++i)
     {
         disconnect(spinBoxes.at(i), 0, 0, 0);
         mapper3->removeMappings(spinBoxes.at(i));
         SAFE_DELETE(spinBoxes.at(i));
     }
     spinBoxes.clear();
     spinBoxes.shrink_to_fit();

     disconnect(mapper, 0, 0, 0);
     disconnect(mapper2, 0, 0, 0);
     disconnect(mapper3, 0, 0, 0);

     itemLayout->removeWidget(labelLocalIP);
     itemLayout->removeWidget(labelIsConnected);

     int rowAlign = 0;
     for(int i = 0; i < audioSessions.size(); ++i)
     {
         labels.push_back(new QLabel());
         labels.back()->setFixedWidth(this->width() * 0.7);
         labels.back()->setFixedHeight(10);
         slider.push_back(new QSlider());
         slider.back()->setFixedWidth(this->width() * 0.6);
         slider.back()->setFixedHeight(25);
         spinBoxes.push_back(new QSpinBox());
         spinBoxes.back()->setFixedWidth(this->width() * 0.2);
         spinBoxes.back()->setFixedHeight(25);

         slider.at(i)->setOrientation(Qt::Horizontal);
         labels.at(i)->setFont(QFont("Arial", 10, QFont::Bold));

         spinBoxes.at(i)->setMinimum(0);
         spinBoxes.at(i)->setMaximum(100);
         spinBoxes.at(i)->setValue(100 * audioSessions.at(i).currentVolumeLevel);

         slider.at(i)->setMinimum(0);
         slider.at(i)->setMaximum(100);
         slider.at(i)->setValue(100 * audioSessions.at(i).currentVolumeLevel);

         labels.at(i)->setText(audioSessions.at(i).displayName.c_str());
         if(labels.at(i)->text().size() == 0)
         {
             labels.at(i)->setText("No name could be found");
         }

         connect(slider.at(i), SIGNAL(valueChanged(int)), mapper, SLOT(map()) );
         connect(spinBoxes.at(i), SIGNAL(editingFinished()), mapper2, SLOT(map()) );
         connect(slider.at(i), SIGNAL(sliderReleased()), mapper3, SLOT(map()));

         mapper->setMapping(slider.at(i), i);
         mapper2->setMapping(spinBoxes.at(i), i);
         mapper3->setMapping(slider.at(i), i);
;
         itemLayout->addWidget(labels.at(i), rowAlign++ , 0);
         itemLayout->addWidget(slider.at(i), rowAlign, 0);
         itemLayout->addWidget(spinBoxes.at(i), rowAlign++, 1);

     }
     scrollArea->setWidget(scrollAreaContent);

     connect(mapper, SIGNAL(mapped(int)), this, SLOT(SliderMoved(int)));
     connect(mapper2, SIGNAL(mapped(int)), this, SLOT(SpinBoxChanged(int)));
     connect(mapper3, SIGNAL(mapped(int)), this, SLOT(SliderReleased(int)));

     parentLayout->addWidget(scrollArea, 0, 0, 1 , 2, Qt::AlignCenter);
     parentLayout->addWidget(labelLocalIP, 1, 0);
     parentLayout->addWidget(labelIsConnected, 1, 1);

}

void MainWindow::SendAudioSession(int index)
{
    if(socket != nullptr)
    {
        if(socket->state() == QAbstractSocket::ConnectedState)
        {
            static S_AudioSession_Network session;
            static char buffer[sizeof(S_AudioSession_Network)];

            session.packetType = pT_audioSession;
            session.index = index;
            session.currentVolumeLevel = float(audioSessions.at(index).currentVolumeLevel) * 100.0f;
            strcpy(session.displayName, audioSessions.at(index).displayName.c_str());
            session.isMuted = audioSessions.at(index).isMuted;

            memcpy(buffer, &session, sizeof(S_AudioSession_Network));

            socket->write(buffer, sizeof(S_AudioSession_Network));
        }
    }
}

void MainWindow::SendAudioSessions()
{
    for(int i = 0; i < audioSessions.size(); ++i)
    {
        static S_AudioSession_Network session;
        static char buffer[sizeof(S_AudioSession_Network)];

        session.packetType = pT_audioSession;
        session.index = i;
        session.currentVolumeLevel = float(audioSessions.at(i).currentVolumeLevel) * 100.0f;
        strcpy(session.displayName, audioSessions.at(i).displayName.c_str());
        session.isMuted = audioSessions.at(i).isMuted;

        memcpy(buffer, &session, sizeof(S_AudioSession_Network));

        if(socket != nullptr)
        {
            if(socket->state() == QAbstractSocket::ConnectedState)
            {
                socket->write(buffer, sizeof(S_AudioSession_Network));
            }
        }
    }
}

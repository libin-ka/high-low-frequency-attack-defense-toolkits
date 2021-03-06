#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>
#include <winscard.h>


#include "about.h"

#pragma comment(lib, "Winscard.lib")

#define HASH_FILE_NAME "Hash.txt"
#define LOG_FILE_NAME  "Log.txt"
#define LOG_SPLIT_LINE \
    "\r\n" \
    "-----------------------------------------------------" \
    "\r\n"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Unicorn EMV Reader v0.3 Serial Port Version");
    setWindowIcon(QIcon(":/images/favicon1.ico"));

    //host = new QPlainTextEdit();
    host = new QTextEdit();
    infoTextEdit = new QPlainTextEdit();

    // 设置顶部栏
    top = new QWidget();
    topLayout  = new QHBoxLayout();
    ReaderCombo = new QComboBox;
    ConnectButton = new QPushButton;
    ReaderCombo->setMaximumHeight(23);
    ReaderCombo->setMaximumWidth(400);
    ReaderCombo->installEventFilter(this);
    ConnectButton->setMaximumHeight(23);
    ConnectButton->setMaximumWidth(90);
    ConnectButton->setMinimumHeight(23);
    ConnectButton->setMinimumWidth(90);
    ConnectButton->setText(QString("Read"));

    Logo = new QLabel;
    QImage image(":/images/LOGO.png");
    Logo->setAlignment(Qt::AlignLeft);
    Logo->setPixmap(QPixmap::fromImage(image));
    Logo->setStyleSheet("background-color: black;");

    topLayout->addWidget(ConnectButton);
    topLayout->addWidget(ReaderCombo);

    top->setLayout(topLayout);

    QLabel *spacelabel = new QLabel;
    spacelabel->setMinimumHeight(20);
    spacelabel->setStyleSheet("QLabel { background-color : black;}");

    Number = new QLabel;
    QFont numberFont("OCR A Extended", 36);
    Number->setStyleSheet("QLabel { background-color : black; color : white; }");
    Number->setMinimumHeight(60);
    Number->setAlignment(Qt::AlignCenter);
    Number->setAlignment(Qt::AlignHCenter);
    Number->setFont(numberFont);

    Validity = new QLabel;
    QFont ValidFont("Microsoft YaHei Mono", 15);
    Validity->setStyleSheet("QLabel { background-color : black; color : white; }");
    Validity->setMinimumHeight(30);
    Validity->setAlignment(Qt::AlignCenter);
    Validity->setAlignment(Qt::AlignHCenter);
    Validity->setFont(ValidFont);

    about = new About;

    QLabel *spacelabel_host = new QLabel;
    spacelabel_host->setMinimumHeight(20);
    spacelabel_host->setStyleSheet("QLabel { background-color : black;}");

    QPalette p_host = host->palette();
    p_host.setColor(QPalette::Base, Qt::black);
    p_host.setColor(QPalette::Text, Qt::white);
    host->setStyleSheet("border: 1px solid black;");
    host->setReadOnly(true);
    QFont msyhmono_host("Microsoft YaHei Mono", 16);
    host->setFont(msyhmono_host);
    host->setPalette(p_host);
    host->setMaximumHeight(180);
    host->setMinimumHeight(180);
    host->setAlignment(Qt::AlignCenter);
    host->setAlignment(Qt::AlignHCenter);

    /*
    QPalette p = infoTextEdit->palette();
    p.setColor(QPalette::Base, Qt::black);
    p.setColor(QPalette::Text, Qt::white);
    infoTextEdit->setStyleSheet("border: 1px solid black;");
    infoTextEdit->setReadOnly(true);
    QFont msyhmono("Microsoft YaHei Mono", 11);
    infoTextEdit->setFont(msyhmono);
    infoTextEdit->setPalette(p);
    */

    mainLayout = new QVBoxLayout();

    QLabel *spacelabel1 = new QLabel;
    spacelabel1->setMinimumHeight(20);
    spacelabel1->setStyleSheet("QLabel { background-color : black;}");

    mainLayout->addWidget(top);
    mainLayout->addWidget(Logo);
    mainLayout->addWidget(spacelabel);
    mainLayout->addWidget(Number);
    mainLayout->addWidget(Validity);
    mainLayout->addWidget(spacelabel_host);
    mainLayout->addWidget(host);
    initInfoAera();
    mainLayout->addWidget(spacelabel1);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0,0,0,0);
    QWidget* widget = new QWidget(this);
    widget->setLayout(mainLayout);
    setCentralWidget(widget);

    initStatusBar();
    InitLogMode();

    //statusLabel = new QLabel;
    //statusLabel->setFont(msyhmono);
    //statusLabel->setAlignment(Qt::AlignLeft);
    //this->statusBar()->addWidget(statusLabel);

    iTimer = new QTimer(this);
    iTimer->setInterval(1000);

    readthread = new ReadThread;
    serial = new QSerialPort();

    initActionsConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object);
    if(event->type() == QEvent::MouseButtonPress) {
        fillReaderInfo();
    }
    return QWidget::eventFilter(object, event);
}

void MainWindow::ProcessData()
{
    switch (CurrentProcess) {
    case 0: {   // 检测读卡器状态
        if(CurrentData.contains("AT+OK")) {
            //readthread->serial = serial;
            status_main->setStyleSheet("QLabel {color : green; }");
            status_main->setText(
                        QString::fromLocal8Bit
                        ("请将银行卡放置于读卡器正面！"));
            //iTimer->start();
            top->hide();
            mainLayout->setSpacing(0);
            mainLayout->setContentsMargins(0,0,0,0);
        } else {
            status_main->setStyleSheet("QLabel {color : red; }");
            status_main->setText(
                        QString::fromLocal8Bit
                        ("未检测到远程读卡器！"));
            serial->close();
        }
        break;
    }
    case 1: { //检测到有数据包传来
        StartReadingInfo();
        break;
    }
    case 2: {
        StartReadingNameEtc();
        break;
    }
    case 3: {
        StartReadingConsumerRecord();
        break;
    }
    default:
        break;
    }
}



void MainWindow::StartReadingConsumerRecord()
{
    //if(!(ItemsNumbers > 0))
    //    return;
    char ConsumerRecordSize = CurrentData[0];
    if(char(CurrentData.size()) != ConsumerRecordSize + 1) {
        ReadItemAlready--;
        qDebug() << "ConsumerRecord Failed, restart again...";
        QString BasicCommand = "AT+";
        char currentKeyWord = '0' + ReadItemAlready;
        CurrentProcess = 3;
        QString AllCommand = BasicCommand + currentKeyWord;
        ReadItemAlready++;
        serial->write(QByteArray(AllCommand.toLatin1()));
        return;
    }

    CUSTUMRECORD Record;
    Record.Money = "";
    Record.Position = "";
    Record.Time = "";
    Record.Type = "";
    BYTE * buffer = new BYTE[45];
    ZeroMemory(buffer, 45);
    unsigned int real_size = CurrentData.size() - 6;
    for(unsigned int i = 0; i < real_size; i++) {
        buffer[i] = CurrentData[i + 6];
    }
    // 打印交易时间
    Record.Time = "";
    //Record.Time += QString::fromLocal8Bit("交易时间： ");
    QString temp;
    temp.sprintf("20%02X", buffer[0]);
    temp += QString::fromLocal8Bit("年");
    Record.Time += temp;
    temp.sprintf("%02X", buffer[1]);
    temp += QString::fromLocal8Bit("月");
    Record.Time += temp;
    temp.sprintf("%02X", buffer[2]);
    temp += QString::fromLocal8Bit("日");
    Record.Time += temp;

    temp = "";
    temp.sprintf(" %02X:%02X:%02X",
        buffer[3], buffer[4], buffer[5]);
    Record.Time += temp;

    // 打印交易金额
    Record.Money = "";
    //Record.Money += QString::fromLocal8Bit("交易金额： ￥");
    Record.Money += QString::fromLocal8Bit("￥");
    BYTE  money[5] = {0};
    BYTE * current = money;

    for (int i = 0; i < 5; i++)
        *(money + i) = *(buffer + 6 + i);
    while (*current == 0)
        current++;

    if (money[0] == 0 && money[1] == 0 && money[2] == 0 && money[3] == 0 && money[4] == 0) {
    //if (current - money == 5) {
        Record.Money += "0";
    }
    else {
        temp = "";
        temp.sprintf("%X", current[0]);
        Record.Money += temp;
        for (current += 1; current - money < 5; current++) {
            temp.sprintf("%02X", current[0]);
            Record.Money += temp;
        }
    }
    temp = "";
    temp.sprintf(".%02X", buffer[12]);
    Record.Money += temp;

    // 打印交易地点
    char * place = new char[20];
    ZeroMemory(place, 20);
    for (int i = 0; i < 20; i++)
        *(place + i) = *(buffer + 22 + i);

    QTextCodec *codec = QTextCodec::codecForName("GBK");
    temp.clear();
    temp = codec->toUnicode(place);
    Record.Position.clear();
    //Record.Position += QString::fromLocal8Bit("交易地点： ");
    //temp = QString::fromLocal8Bit(place);
    Record.Position += temp;
    int iLength = Record.Position.toLocal8Bit().length();
    if(iLength >= 16) {
        for (int i = 0; i < 19; i++)
            *(place + i) = *(buffer + 22 + i);
        place[19] = 0x00;
        Record.Position = QString::fromLocal8Bit(place);;
    }

    // 打印交易类型
    Record.Type = "";
    if (buffer[42] == 0x01) {
        Record.Type += QString::fromLocal8Bit("提款/现金付款");
    }
    else if (buffer[42] == 0x30) {
        Record.Type += QString::fromLocal8Bit("查询可用现金");
    }
    else {
        Record.Type += QString::fromLocal8Bit("POS机交易");
    }

    //RecordList.push_back(Record);
    delete []place;
    delete []buffer;
    qDebug() << Record.Time ;
    qDebug() << Record.Money;
    qDebug() << Record.Position;
    qDebug() << Record.Type;

    QStringList temp_list;
    temp_list << Record.Time
         << Record.Money
         << Record.Position
         << Record.Type;
    AddInfoList(temp_list);

    all_records += Record.Time;
    all_records += "\t";
    //all_records += Record.Money;
    unsigned int space_size = 15 - Record.Money.length();
    all_records += Record.Money;
    all_records += QString(space_size, ' ');
    //all_records += "\t\t";
    all_records += Record.Position;
    all_records += "\t";
    all_records += Record.Type;
    all_records += "\r\n";

    if(ReadItemAlready < ItemsNumbers) {
        QString BasicCommand = "AT+";
        char currentKeyWord = '0' + ReadItemAlready;
        ReadItemAlready++;
        CurrentProcess = 3;
        QString AllCommand = BasicCommand + currentKeyWord;
        qDebug() << "Current Command:";
        qDebug() << AllCommand;
        qDebug() << "ItemsNumbers:";
        qDebug() << QString::number(ItemsNumbers);
        qDebug() << "ReadItemAlready:";
        qDebug() << QString::number(ReadItemAlready);
        serial->write(QByteArray(AllCommand.toLatin1()));
    }

    if(ReadItemAlready == ItemsNumbers){
        if(true == bLog) {
            WriteLog();
        }
    }
}

void MainWindow::WriteLog(void)
{
    qDebug() << "Writing Log file!";
    // read all record, ready to write into log
    QString CurrentLog = "";
    if(!guy_name.isEmpty()) {
        CurrentLog += QString::fromLocal8Bit("持卡人姓名： ");
        CurrentLog += guy_name;
        CurrentLog += "\r\n";
    }

    if(!guy_name_ex.isEmpty()) {
        CurrentLog += QString::fromLocal8Bit("持卡人姓名扩展： ");
        CurrentLog += guy_name_ex;
        CurrentLog += "\r\n";
    }

    if(!id_num.isEmpty()) {
        CurrentLog += QString::fromLocal8Bit("持卡人身份信息： ");
        CurrentLog += id_num;
        CurrentLog += "\r\n";
    }

    if(!id_type.isEmpty()) {
        CurrentLog += QString::fromLocal8Bit("持卡人身份信息类型： ");
        CurrentLog += id_type;
        CurrentLog += "\r\n";
    }

    if(!card_name.isEmpty()) {
        CurrentLog += QString::fromLocal8Bit("发卡行： ");
        CurrentLog += card_name;
        CurrentLog += "\r\n";
    }

    if(!begin_time.isEmpty()) {
        CurrentLog += QString::fromLocal8Bit("卡片有效期： ");
        CurrentLog += begin_time;
        CurrentLog += QString::fromLocal8Bit(" - ");
        CurrentLog += end_time;
        CurrentLog += "\r\n";
    }

    if(!card_type.isEmpty()) {
        CurrentLog += QString::fromLocal8Bit("持卡类型： ");
        CurrentLog += card_type;
        CurrentLog += "\r\n";
    }

    if(!card_num.isEmpty()) {
        CurrentLog += QString::fromLocal8Bit("卡号： ");
        CurrentLog += card_num;
        CurrentLog += "\r\n";
    }

    CurrentLog += QString::fromLocal8Bit("交易时间：");
    CurrentLog += "\t";
    CurrentLog += QString::fromLocal8Bit("交易金额：");
    CurrentLog += "\t";
    CurrentLog += QString::fromLocal8Bit("交易地点：");
    CurrentLog += "\t";
    CurrentLog += QString::fromLocal8Bit("交易类型：");
    CurrentLog += "\r\n";

    CurrentLog += all_records;

    qDebug() << "Log: ";
    qDebug() << CurrentLog.toLatin1().toHex();

    QString CurrentHash = QString(QCryptographicHash::hash(CurrentLog.toLocal8Bit(), QCryptographicHash::Md5).toHex());
    qDebug() << "Hash: ";
    qDebug() << CurrentHash;

    if(!LogHashList.contains(CurrentHash)) {
        CurrentLog += LOG_SPLIT_LINE;
        QFile log_file(LOG_FILE_NAME);
        if(!log_file.open(QIODevice::Append))
            return;
        QFile hash_file(HASH_FILE_NAME);
        if(!hash_file.open(QIODevice::Append))
            return;
        QTextStream log_out(&log_file);
        log_out << CurrentLog;
        QTextStream hash_out(&hash_file);
        hash_out << CurrentHash << "\r\n";
        LogHashList.push_back(CurrentHash);
        log_file.close();
        hash_file.close();
    }
}

void MainWindow::InitHashMap(void)
{
    QFile file(HASH_FILE_NAME);
    LogHashList.clear();
    if(!file.open(QIODevice::ReadWrite)) {
        qDebug() << QString("Error in opening file, seems cant write file here");
        return;
    }

    QTextStream in(&file);
    while(!in.atEnd()) {
        QString hash = in.readLine();
        LogHashList << hash;
    }
    file.close();
}

void MainWindow::StartReadingInfo()
{
    qDebug() << "Already got data, ganna parse them~";
    serial->write("AT+0");
    CurrentProcess = 2;
}

void MainWindow::StartReadingNameEtc()
{
    qDebug() << "Parsing Bankcard Name and basic infos else:";
    //qDebug() << "Original Data: " ;
    //qDebug() << CurrentData.toHex();

    guy_name = "";
    guy_name_ex = "";
    id_num = "";
    id_type = "";
    card_name = "";
    begin_time = "";
    end_time = "";
    card_type = "";
    card_num = "";
    all_records = "";
    ItemsNumbers = 0;
    ReadItemAlready = 1;

    int TotalLenth = CurrentData.at(0);
    qDebug() << "Total Length: " << CurrentData.size() << " Self Define Length: " << TotalLenth;
    //int IngoreID        = CurrentData.at(1); // i dont care
    //int GroupSquence    = CurrentData.at(2); // 组序列号
    //int SelfSquence     = CurrentData.at(3); // 单个序列号
    ItemsNumbers     = CurrentData.at(4); // 分数据长度
    int ReadDataLength  = CurrentData.at(5); // 实际所有数据长度

    QByteArray RealData = CurrentData.mid(6);
    if(RealData.size() != ReadDataLength ) {
        qDebug() << "it seems there is no complete data!";
        status_main->setStyleSheet("QLabel {color : red; }");
        status_main->setText(
                    QString::fromLocal8Bit
                    ("基本信息读取被截断，请重新读取或者联系开发人员！"));
        CurrentProcess = 1;
        return;
    } else {
        ClearText();
        ProcessRealData(RealData);
        qDebug() << "Card Num: ";
        qDebug() << card_num;
        PrintCardNum(card_num);
        if(!card_type.isEmpty())
            PrintCardType(card_type);
        if(!card_name.isEmpty())
            PrintCardName(card_name);
        if(!id_num.isEmpty() && !id_type.isEmpty())
            PrintIdType(id_type);
        PrintValidity(begin_time, end_time);
TheEnd:
        qDebug() << "The End!" ;
    }

    //for(unsigned int i = 0; i < ItemsNumbers - 1; i++) {
    //    QString BasicCommand = "AT+";
    //    char currentKeyWord = '1' + i;
    //    CurrentProcess = 3;
    //    QString AllCommand = BasicCommand + currentKeyWord;
    //    serial->write(QByteArray(AllCommand.toLatin1()));
    //}
    QStringList title;
    title << QString::fromLocal8Bit("交易时间：")
          << QString::fromLocal8Bit("交易金额：")
          << QString::fromLocal8Bit("交易地点：")
          << QString::fromLocal8Bit("交易类型：");
    AddInfoList(title);

    if(ReadItemAlready < ItemsNumbers) {
        QString BasicCommand = "AT+";
        char currentKeyWord = '0' + ReadItemAlready;
        ReadItemAlready++;
        CurrentProcess = 3;
        QString AllCommand = BasicCommand + currentKeyWord;
        serial->write(QByteArray(AllCommand.toLatin1()));
    }
}

void MainWindow::ProcessRealData(QByteArray RealData)
{
    qDebug() << "Processing Data: " ;
    //qDebug() << RealData.toHex();
    if(RealData.size() == 0) {
        return;
    } else {
        unsigned char FirstBit = RealData[0];
        unsigned char SecondBit = RealData[1];

        if(FirstBit == 0x57) { // Bank card number,原来不是5A么？
            unsigned int CardNumberLength = RealData.at(1);
            QByteArray CardNumberPackage = RealData.mid(2, CardNumberLength);
            if(card_num.isEmpty()) {
                //QString temp;
                for (unsigned int i = 0; i < CardNumberLength; i++){
                    //temp.sprintf("%02X", CardNumberPackage[i]);
                    card_num += char(CardNumberPackage[i] + 0x30);
                    qDebug() << "Card Number: " << card_num;
                }
            }
            ProcessRealData(RealData.mid(2+CardNumberLength));
        //} else if (RealData.at(0) == 0x9F && RealData.at(1) == 0x61) {
        //if (RealData.at(21) == 0x9F && RealData.at(22) == 0x61) {
        } else if(FirstBit == 0x9f && SecondBit == 0x61 ) {
            //qDebug() << "Fuck 1234";
            if(id_num.isEmpty()) {
                //qDebug() << "Fuck 123";
                id_num += QString::fromLocal8Bit((RealData.mid(3, RealData.at(2))));
                id_num = id_num.simplified();
                //qDebug() << "Fuck ";
                qDebug() << "ID : " << id_num;
            }
            ProcessRealData(RealData.mid(3 + RealData.at(2)));
        } else if (FirstBit == 0x5F && SecondBit == 0x24) {
            // 过期时间
            if(end_time.isEmpty()) {
                 QString temp;
                 end_time += QString::fromLocal8Bit(std::string("20").c_str());
                 temp.sprintf("%02X", RealData.at(2));
                 end_time += temp;
                 end_time += QString::fromLocal8Bit(std::string(".").c_str());
                 temp.sprintf("%02X", RealData.at(3));
                 end_time += temp;
                 end_time += QString::fromLocal8Bit(std::string(".").c_str());
                 end_time += QString::fromLocal8Bit(std::string("30").c_str());
                 qDebug() << "Expire Time: " << end_time;
            }
            ProcessRealData(RealData.mid(4));
        } else if (FirstBit == 0x5F && SecondBit == 0x25) {
            QString temp = "";
            if(begin_time.isEmpty()) {
               begin_time += QString::fromLocal8Bit(std::string("20").c_str());
               temp.sprintf("%02X", RealData.at(3));
               begin_time += temp;
               begin_time += QString::fromLocal8Bit(std::string(".").c_str());
               temp.sprintf("%02X", RealData.at(4));
               begin_time += temp;
               begin_time += QString::fromLocal8Bit(std::string(".").c_str());
               temp.sprintf("%02X", RealData.at(5));
               begin_time += temp;
               qDebug() << "Start Time: " << begin_time;
            }
            ProcessRealData(RealData.mid(6));
        } else if (FirstBit == 0x5F && SecondBit == 0x26) {
            if(end_time.isEmpty()) {
                QString temp = "";
               end_time += QString::fromLocal8Bit(std::string("20").c_str());
               temp.sprintf("%02X", RealData.at(3));
               end_time += temp;
               end_time += QString::fromLocal8Bit(std::string(".").c_str());
               temp.sprintf("%02X", RealData.at(4));
               end_time += temp;
               end_time += QString::fromLocal8Bit(std::string(".").c_str());
               temp.sprintf("%02X", RealData.at(5));
               end_time += temp;
               qDebug() << "Expire Time: " << end_time;
            }
            ProcessRealData(RealData.mid(6));
        }  else if (FirstBit == 0x9F && SecondBit == 0x0b) {
            if (guy_name_ex.isEmpty()) {
                guy_name_ex += QString::fromLocal8Bit((RealData.mid(3, RealData.at(2))));
                guy_name_ex = guy_name_ex.simplified();
                qDebug() << "Name ex: " << guy_name_ex;
            }
            ProcessRealData(RealData.mid(3 + RealData.at(2)));
        } else if (FirstBit == 0x5F && SecondBit == 0x20) {
            if (guy_name.isEmpty()) {
                guy_name += QString::fromLocal8Bit((RealData.mid(3, RealData.at(2))));
                guy_name = guy_name.simplified();
                qDebug() << "Name ex: " << guy_name_ex;
            }
            ProcessRealData(RealData.mid(3 + RealData.at(2)));
        }
        else {
            qDebug() << "Failed";
            QString first;
            first.setNum(FirstBit,16);
            QString second;
            second.setNum(SecondBit,16);
            qDebug() << first;
            qDebug() << second;
            return;
        }
    }
}

void MainWindow::readData()
{
    CurrentData.clear();
    CurrentData = serial->readAll();
    qDebug() << "Read data: " << CurrentData.toHex();
    if(CurrentData.contains("AT+GET")) {
        CurrentProcess = 1;
    }
    ProcessData();
}

void MainWindow::apply()
{
    //ReaderName = ReaderCombo->currentText();
    //if(ReaderName.isEmpty())
    //    return;
    int idx = ReaderCombo->currentIndex();
    PortName = ReaderCombo->itemData(idx).toStringList().at(0);
    serial->setPortName(PortName);
    serial->setBaudRate(QSerialPort::Baud115200);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);
    //serial->setReadBufferSize(0);
    if(serial->open(QIODevice::ReadWrite)) {
        //readthread->Reader = ReaderName;
        //readthread->hSC = hSC;
        // 检测是否存在远程读卡器
        //serial->readAll();
        serial->write("AT+C");
        CurrentProcess = 0;
        /*
        if(data.contains("AT+OK")) {
            readthread->serial = serial;
            status_main->setStyleSheet("QLabel {color : green; }");
            status_main->setText(
                        QString::fromLocal8Bit
                        ("请将银行卡放置于读卡器正面！"));
            iTimer->start();
            top->hide();
            mainLayout->setSpacing(0);
            mainLayout->setContentsMargins(0,0,0,0);
        } else {
            status_main->setStyleSheet("QLabel {color : red; }");
            status_main->setText(
                        QString::fromLocal8Bit
                        ("未检测到远程读卡器！"));
            serial->close();
        }
        */
        //FakeRecords();

    } else {
        status_main->setStyleSheet("QLabel {color : red; }");
        status_main->setText(
                    QString::fromLocal8Bit
                    ("无法打开串口，请检查是否被占用！"));
    }
}

void MainWindow::Timeout()
{
    readthread->start();
}

bool MainWindow::fillReaderInfo()
{
    ReaderCombo->clear();
    static const QString blankString = QObject::tr("N/A");
    QString description;
    QString manufacturer;
    QString serialNumber;
    Q_FOREACH (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        QStringList list;
        description = info.description();
        manufacturer = info.manufacturer();
        serialNumber = info.serialNumber();
        list << info.portName()
             << (!description.isEmpty() ? description : blankString)
             << (!manufacturer.isEmpty() ? manufacturer : blankString)
             << (!serialNumber.isEmpty() ? serialNumber : blankString)
             << info.systemLocation()
             << (info.vendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : blankString)
             << (info.productIdentifier() ? QString::number(info.productIdentifier(), 16) : blankString);

        ReaderCombo->addItem(list.first() + " -> " + list.at(1), list);
    }
    return TRUE;
}

void MainWindow::initInfoAera()
{
    QFont msyhmono("Microsoft YaHei Mono", 14);
    //InfoAera = new QWidget;
    for(int i = 0; i < 11; i++) {
        QHBoxLayout *singal = new QHBoxLayout;
        INFOLABEL singalLabel;
        singalLabel.Whole = new QWidget;
        singalLabel.LabelMoney = new QLabel;
        singalLabel.LabelPos = new QLabel;
        singalLabel.LabelTime = new QLabel;
        singalLabel.LabelType = new QLabel;
        singalLabel.LabelMoney->setMinimumHeight(10);
        singalLabel.LabelPos->setMinimumHeight(10);
        singalLabel.LabelTime->setMinimumHeight(10);
        singalLabel.LabelType->setMinimumHeight(10);
        singalLabel.LabelMoney->setStyleSheet("QLabel { background-color : black; color : white;}");
        singalLabel.LabelPos->setStyleSheet("QLabel { background-color : black; color : white;}");
        singalLabel.LabelTime->setStyleSheet("QLabel { background-color : black; color : white;}");
        singalLabel.LabelType->setStyleSheet("QLabel { background-color : black; color : white;}");
        singalLabel.LabelMoney->setFont(msyhmono);
        singalLabel.LabelPos->setFont(msyhmono);
        singalLabel.LabelTime->setFont(msyhmono);
        singalLabel.LabelType->setFont(msyhmono);
        singalLabel.LabelMoney->setAlignment(Qt::AlignCenter);
        singalLabel.LabelPos->setAlignment(Qt::AlignCenter);
        singalLabel.LabelTime->setAlignment(Qt::AlignCenter);
        singalLabel.LabelType->setAlignment(Qt::AlignCenter);
        singal->addWidget(singalLabel.LabelTime);
        singal->addWidget(singalLabel.LabelMoney);
        singal->addWidget(singalLabel.LabelPos);
        singal->addWidget(singalLabel.LabelType);
        InfoList.push_back(singalLabel);
        singal->setSpacing(0);
        singal->setContentsMargins(0,0,0,0);
        singalLabel.Whole->setLayout(singal);
        mainLayout->addWidget(singalLabel.Whole);
    }
    ListIndex = InfoList.begin();
}

void MainWindow::ClearText()
{
    Validity->setText("");
    Number->setText("");
    host->setPlainText("");
    infoTextEdit->setPlainText("");

    QList<INFOLABEL>::Iterator Index;
    for(Index = InfoList.begin(); Index != InfoList.end(); Index++) {
        Index->LabelTime->setText("");
        Index->LabelMoney->setText("");
        Index->LabelPos->setText("");
        Index->LabelType->setText("");
    }
    ListIndex = InfoList.begin();
}

void MainWindow::AddText(QString str)
{
    infoTextEdit->insertPlainText(str);
}

void MainWindow::AddInfoList(QStringList list)
{
    if(ListIndex < InfoList.end()) {
        ListIndex->LabelTime->setText(list.at(0));
        ListIndex->LabelMoney->setText(list.at(1));
        ListIndex->LabelPos->setText(list.at(2));
        ListIndex->LabelType->setText(list.at(3));

        ListIndex++;
    }
}


void MainWindow::ShowStatus(QString status, int Level)
{
    if(Level == STATUS_LEVEL_NORMAL)
        status_main->setStyleSheet("QLabel {color : green; }");
    else if (Level == STATUS_LEVEL_WARNING)
        status_main->setStyleSheet("QLabel {color : red; }");

    status_main->setText(status);
}

void MainWindow::PrintCardNum(QString cardnum)
{
    if(cardnum.length() == 0)
        ShowStatus(QString::fromLocal8Bit("卡号读取有误！请联系开发人员！"), STATUS_LEVEL_WARNING);

    if(bMasaicsMode == true) {
        int iLength = cardnum.length();
        int iMasaicsLength = iLength - 8;
        QString MasaicsString = QString(iMasaicsLength, '*');
        cardnum.replace(8, iMasaicsLength, MasaicsString);
    }

    if(cardnum.endsWith('F', Qt::CaseInsensitive)){
        cardnum.chop(1);
    }

    //cardnum.insert(0, ' ');
    int iLength = cardnum.length();
    //for( int i = 1; i < iLength; i++) {
    //    if(i % 5 == 0) {
    //        cardnum.insert(i, ' ');
    //    }
    //}

    QString new_num;
    for(int i = 0; i < iLength; i++) {
        if(i % 4 == 0) {
            new_num += cardnum.mid(i, 4);
            if(i + 4 != iLength)
                new_num += ' ';
        }
    }


    //cardnum.insert(0, QString::fromLocal8Bit("卡号："));
    //Number->setText(cardnum);
    Number->setText(new_num);
}

void MainWindow::PrintCardType(QString cardType)
{
    if(cardType.contains("DEBIT")) {
        cardType += QString::fromLocal8Bit("（借记卡）");
    } else if (cardType.contains("CREDIT")) {
        cardType += QString::fromLocal8Bit("（信用卡）");
    }

    cardType.insert(0, QString::fromLocal8Bit("\t持卡类型："));
    cardType.append('\n');
    host->insertPlainText(cardType);
}

void MainWindow::PrintCardName(QString cardName)
{
    cardName.insert(0, QString::fromLocal8Bit("\t卡名："));
    cardName.append('\n');
    host->insertPlainText(cardName);
}

void MainWindow::PrintGuyName(QString guyName)
{
    if(bMasaicsMode == true) {
        //int iLength = guyName.length();
        QByteArray temp = guyName.toLocal8Bit();
        int iLength = temp.length();
        int iMasaicsLength = iLength - 2;
        QByteArray MasaicsByte = QByteArray(iMasaicsLength, '*');
        temp.replace(2, iMasaicsLength, MasaicsByte);
        guyName = QString::fromLocal8Bit(temp);
        //QString MasaicsString = QString(iMasaicsLength, '*');
        //guyName.replace(2, iMasaicsLength, MasaicsString);
    }
    guyName.insert(0, QString::fromLocal8Bit("\t持卡人姓名："));
    guyName.append('\n');
    host->insertPlainText(guyName);
}

void MainWindow::PrintGuyNameEx(QString guyNameEx)
{
    if(bMasaicsMode == true) {
        int iLength = guyNameEx.length();
        int iMasaicsLength = iLength - 2;
        QString MasaicsString = QString(iMasaicsLength, '*');
        guyNameEx.replace(2, iMasaicsLength, MasaicsString);
    }
    guyNameEx.insert(0, QString::fromLocal8Bit("\t持卡人姓名扩展："));
    guyNameEx.append('\n');
    host->insertPlainText(guyNameEx);
}

void MainWindow::PrintIdNum(QString idNum)
{
    if(bMasaicsMode == true) {
        int iLength = idNum.length();
        int iMasaicsLength = iLength - 6;
        QString MasaicsString = QString(iMasaicsLength, '*');
        idNum.replace(6, iMasaicsLength, MasaicsString);
    }
    idNum.insert(0, QString::fromLocal8Bit("\t持卡人证件号："));
    host->insertPlainText(idNum);
}

void MainWindow::PrintIdType(QString idType)
{
    idType.insert(0, QString::fromLocal8Bit("\t"));
    idType.append('\n');
    host->insertPlainText(idType);
}

void MainWindow::PrintValidity(QString from, QString to)
{
    if(bMasaicsMode == true) {
        int iLength = from.length();
        if(iLength <= 8) {
            iLength = to.length();
        }
        int iMasaicsLength = iLength - 8;
        QString MasaicsString = QString(iMasaicsLength, '*');
        from.replace(8, iMasaicsLength, MasaicsString);
        to.replace(8, iMasaicsLength, MasaicsString);
    }

    QString temp = QString::fromLocal8Bit("卡有效期：");
    temp += from;
    temp += " - ";
    temp += to;
    Validity->setText(temp);
}

void MainWindow::initStatusBar()
{
    bMasaicsMode = false;
    //bLog = true;
    status_main = new QLabel(this);
    status_main->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    status_main->setFont(QFont("Microsoft YaHei Mono", 11));
    status_main->setAlignment(Qt::AlignLeft);

    QPalette pa;
    pa.setColor(QPalette::WindowText,Qt::gray);
    status_masaics = new QLabel(QString::fromLocal8Bit("马赛克模式"),this);
    status_masaics->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    status_masaics->setPalette(pa);
    status_masaics->setFont(QFont("Microsoft YaHei Mono", 11));
    status_masaics->setAlignment(Qt::AlignCenter);

    statusBar()->addPermanentWidget(status_main, 12);
    statusBar()->addPermanentWidget(status_masaics, 1);
}

void MainWindow::ChangePrivacyStatus()
{
    bMasaicsMode = !bMasaicsMode;
    if(bMasaicsMode == true) {
        QPalette pa;
        pa.setColor(QPalette::WindowText,Qt::red);
        status_masaics->setPalette(pa);
    } else {
        QPalette pa;
        pa.setColor(QPalette::WindowText,Qt::gray);
        status_masaics->setPalette(pa);
    }
}

void MainWindow::InitLogMode(void)
{
    bLog = true;
    InitHashMap();
    ui->actionStartLog->setChecked(true);
}

void MainWindow::changeLogMode(void)
{
    bLog = !bLog;
}

void MainWindow::showLog(void)
{
    QString current_dir = QDir::currentPath();
    current_dir += "\\Log.txt";
    QString total_command = "notepad.exe " + current_dir;
    QProcess::startDetached(total_command);
}

void MainWindow::delLog()
{
    QFile file(LOG_FILE_NAME);
    file.close();
    file.remove();
    QFile HASH_file(HASH_FILE_NAME);
    HASH_file.close();
    HASH_file.remove();
    LogHashList.clear();
}

void MainWindow::initActionsConnections()
{
    connect(ui->actionClear, SIGNAL(triggered()), this, SLOT(ClearText()));
    connect(ui->actionMosaics, SIGNAL(triggered()), this, SLOT(ChangePrivacyStatus()));
    connect(ui->actionAbout, SIGNAL(triggered()), about, SLOT(show()));
    connect(ui->actionStartLog, SIGNAL(triggered()), this, SLOT(changeLogMode()));
    connect(ui->actionShowLog, SIGNAL(triggered(bool)), this, SLOT(showLog()));
    connect(ui->actionClearLog, SIGNAL(triggered(bool)),this, SLOT(delLog()));
    connect(ConnectButton, SIGNAL(clicked()), this, SLOT(apply()));
    connect(iTimer, SIGNAL(timeout()), this, SLOT(Timeout()));
    //connect(infoTextEdit, SIGNAL(textChanged()), this, SLOT(textChanged()));
    connect(serial, SIGNAL(readyRead()), this, SLOT(readData()));


    int id = qRegisterMetaType<QString>("");
    id = qRegisterMetaType<QStringList>("");
    connect(readthread, SIGNAL(ClearText()), this, SLOT(ClearText()));
    connect(readthread, SIGNAL(AddText(QString)), this, SLOT(AddText(QString)), Qt::QueuedConnection);
    connect(readthread, SIGNAL(AddInfoList(QStringList)), this, SLOT(AddInfoList(QStringList)));
    connect(readthread, SIGNAL(ShowStatus(QString, int)), this, SLOT(ShowStatus(QString, int)));
    connect(readthread, SIGNAL(PrintCardNum(QString)), this, SLOT(PrintCardNum(QString)));
    connect(readthread, SIGNAL(PrintCardType(QString)), this, SLOT(PrintCardType(QString)));
    connect(readthread, SIGNAL(PrintCardName(QString)), this, SLOT(PrintCardName(QString)));
    connect(readthread, SIGNAL(PrintGuyName(QString)), this, SLOT(PrintGuyName(QString)));
    connect(readthread, SIGNAL(PrintGuyNameEx(QString)), this, SLOT(PrintGuyNameEx(QString)));
    connect(readthread, SIGNAL(PrintIdNum(QString)), this, SLOT(PrintIdNum(QString)));
    connect(readthread, SIGNAL(PrintIdType(QString)), this, SLOT(PrintIdType(QString)));
    connect(readthread, SIGNAL(PrintValidity(QString,QString)), this, SLOT(PrintValidity(QString,QString)));
}

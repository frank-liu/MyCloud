#include "QtNewSyncDialog.h"

QtNewSyncDialog::QtNewSyncDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
	myparent = parent;
	performer = parentperf;
	netibuf = parentibuf;
	netobuf = parentobuf;
	ui.sendLabel->setVisible(false);
}

QtNewSyncDialog::~QtNewSyncDialog()
{
}

void QtNewSyncDialog::showEvent(QShowEvent *event){
	QDialog::showEvent(event);
}

void QtNewSyncDialog::accept(){
	int rc;
	if(ui.nameEdit->text().length() == 0){
		QMessageBox b(this);
		b.setText(tr("No name set"));
		b.setInformativeText(tr("Please choose a name for the new Sync."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		return;
	}
	
	sync.id = MC_SYNCID_NONE;
	sync.crypted = ui.encryptedBox->isChecked();
	sync.filterversion = 0;
	sync.name = qPrintable(ui.nameEdit->text());
	memset(sync.hash,0,16);
	

	connect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	srv_createsync_async(netibuf,netobuf,performer,sync.name,sync.crypted);

	
	ui.nameEdit->setEnabled(false);
	ui.encryptedBox->setEnabled(false);
	ui.okButton->setVisible(false);
	ui.sendLabel->setVisible(true);

	//replyReceived does the accept
}

void QtNewSyncDialog::replyReceived(int rc){
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	if(rc){
		reject();
		return;
	}

	rc = srv_createsync_process(netobuf,&sync.id);
	if(rc){
		reject();
		return;
	}

	// donwload keyring
	if(sync.crypted){
		ui.sendLabel->setText(tr("<i>downloading keyring...</i>"));
		connect(performer,SIGNAL(finished(int)),this,SLOT(keyringReceived(int)));
		srv_getkeyring_async(netibuf,netobuf,performer);
	} else {
		QDialog::accept();
	}
}

void QtNewSyncDialog::keyringReceived(int rc){
	string keyringdata;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(keyringReceived(int)));
	if(rc) {
		reject();
		return;
	}

	rc = srv_getkeyring_process(netobuf,&keyringdata);
	if(rc){
		reject();
		return;
	}

	if(keyringdata.length() > 0){ // don't ask for a password when there is no ring...

		bool ok = false;
		while(!ok){
			QString pass = "";
			while(!ok){
				pass = QInputDialog::getText(this, tr("Keyring Password"), tr("Please enter the password to your keyring"), QLineEdit::Password, NULL, &ok, windowFlags() & ~Qt::WindowContextHelpButtonHint);
			}

			// decrypt
			rc = crypt_keyring_fromsrv(keyringdata,pass.toStdString(),&keyring);
			if(rc){
				QMessageBox b(this);
				b.setText(tr("Keyring Decryption failed"));
				b.setInformativeText(tr("The keyring could not be decrypted! Re-check your password or enter the key manually."));
				b.setStandardButtons(QMessageBox::Ok);
				b.setDefaultButton(QMessageBox::Ok);
				b.setIcon(QMessageBox::Critical);
				b.exec();
				ui.okButton->setEnabled(true);
				ok = false;
			}
		}
	}

	// generate new key
	ui.sendLabel->setText(tr("<i>generating key...</i>"));

	QByteArray newkey = QByteArray(32,'\0');
	while(crypt_randkey((unsigned char*)newkey.data())){
		QMessageBox b(this);
		b.setText(tr("Can't generate keys atm"));
		b.setInformativeText(tr("Please do something else so the system can collect entropy"));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		ui.okButton->setEnabled(true);
		return;
	} 

		int result = 0;

		// TODO: add to keyring and send to server
		bool found = false;
		for(mc_keyringentry& entry : keyring){
			if(entry.sid == sync.id){
				entry.sname = sync.name;
				memcpy(entry.key,newkey.constData(),newkey.size());
				found = true;
			}
		}
		if(!found){
			mc_keyringentry newentry;
			newentry.sid = sync.id;
			newentry.sname = sync.name;
			memcpy(newentry.key,newkey.constData(),newkey.size());
			keyring.push_back(newentry);
		}

			
		bool ok = false;
		QString pass = "";
		while(!ok){
			pass = QInputDialog::getText(this, tr("Keyring Password"), tr("Please enter the new password for your keyring.\nIt is used to encrypt the keyring and should not be related to your account/server password!"), QLineEdit::Password, NULL, &ok, windowFlags() & ~Qt::WindowContextHelpButtonHint);
		}

		rc = crypt_keyring_tosrv(&keyring,pass.toStdString(),&keyringdata);
		if(rc){
			reject();
			return;
		}

		ui.sendLabel->setText(tr("<i>uploading keyring...</i>"));
		connect(performer,SIGNAL(finished(int)),this,SLOT(keyringSent(int)));
		srv_setkeyring_async(netibuf,netobuf,performer,&keyringdata);

	return;
	
}

void QtNewSyncDialog::keyringSent(int rc){
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(keyringSent(int)));
	
	if(rc) {
		reject();
		return;
	}

	rc = srv_setkeyring_process(netobuf);
	if(rc){
		reject();
		return;
	}
	
	QDialog::accept();
}
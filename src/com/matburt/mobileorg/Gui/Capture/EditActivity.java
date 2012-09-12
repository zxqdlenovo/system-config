package com.matburt.mobileorg.Gui.Capture;

import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.EditText;
import android.widget.LinearLayout;

import com.actionbarsherlock.app.SherlockFragmentActivity;
import com.actionbarsherlock.view.Menu;
import com.actionbarsherlock.view.MenuInflater;
import com.actionbarsherlock.view.MenuItem;
import com.actionbarsherlock.view.SubMenu;
import com.matburt.mobileorg.R;
import com.matburt.mobileorg.OrgData.OrgNode;
import com.matburt.mobileorg.Services.TimeclockService;
import com.matburt.mobileorg.Synchronizers.Synchronizer;
import com.matburt.mobileorg.util.OrgUtils;

public class EditActivity extends SherlockFragmentActivity {
	public final static String NODE_ID = "node_id";
	public final static String ACTIONMODE = "actionMode";
	public final static String ACTIONMODE_CREATE = "create";
	public final static String ACTIONMODE_EDIT = "edit";
	public final static String ACTIONMODE_ADDCHILD = "add_child";

	private OrgNode node;
	private String actionMode;

	private ContentResolver resolver;
	private LinearLayout payloadView;

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		setContentView(R.layout.edit);
		
		this.resolver = getContentResolver();
		
		this.payloadView = (LinearLayout) findViewById(R.id.edit_payload);
		this.payloadView.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				payloadView.removeAllViews();
				EditText payloadEdit = new EditText(getBaseContext());
				payloadEdit.setText(node.getRawPayload());
				payloadView.addView(payloadEdit);
			}
		});		
		
		initState();
	}
	
	private void initState() {
		Intent intent = getIntent();
		this.actionMode = intent.getStringExtra("actionMode");
		long node_id = intent.getLongExtra("node_id", -1);	
		
		if (this.actionMode == null) {
			this.node = OrgUtils.getCaptureIntentContents(intent);
			this.actionMode = ACTIONMODE_CREATE;
		} else if (this.actionMode.equals(ACTIONMODE_CREATE)) {
			this.node = new OrgNode();
		} else if (this.actionMode.equals(ACTIONMODE_EDIT)) {
			this.node = new OrgNode(node_id, getContentResolver());
		} else if (this.actionMode.equals(ACTIONMODE_ADDCHILD)) {
			this.node = new OrgNode();
			this.node.parentId = node_id;
		}
	}

	
	public OrgNode getOrgNode() {
		if(this.node == null)
			this.node = new OrgNode();
		return this.node;
	}
	
    @Override
	public boolean onPrepareOptionsMenu(Menu menu) {
    	super.onCreateOptionsMenu(menu);
		MenuInflater inflater = getSupportMenuInflater();
	    inflater.inflate(R.menu.edit, menu);
	    
	    if(this.node == null)
	    	return true;
	    
		if (this.node.id > -1) {
			SubMenu subMenu = menu.addSubMenu(R.string.menu_advanced);
			MenuItem item = subMenu.add(R.string.menu_advanced,
					R.string.menu_delete, 0, R.string.menu_delete);
			item.setIcon(R.drawable.ic_input_delete);

			item = subMenu.add(R.string.menu_advanced, R.string.menu_clockin,
					1, R.string.menu_clockin);
			item.setIcon(R.drawable.ic_menu_today);
			
			item = subMenu.add(R.string.menu_advanced, R.string.menu_archive,
					1, R.string.menu_archive);
			
			item = subMenu.add(R.string.menu_advanced, R.string.menu_archive_tosibling,
					1, R.string.menu_archive_tosibling);

			MenuItem subMenuItem = subMenu.getItem();
			subMenuItem.setIcon(R.drawable.ic_menu_moreoverflow);
			subMenuItem.setShowAsAction(MenuItem.SHOW_AS_ACTION_ALWAYS);
		}
		return true;	
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {
		case android.R.id.home:
			doCancel();
			return true;
			
		case R.id.nodeedit_save:
			saveEdits();
			setResult(RESULT_OK);
			finish();
			return true;
			
		case R.id.nodeedit_cancel:
			doCancel();
			return true;
			
		case R.string.menu_delete:
			runDeleteNode();
			return true;
			
		case R.string.menu_clockin:
			runTimeClockingService();
			return true;
			
		case R.string.menu_archive:
			runArchiveNode("archive");
			return true;
			
		case R.string.menu_archive_tosibling:
			runArchiveNode("archive-sibling");
			return true;		
		}
		return false;
	}
	
	private void runDeleteNode() {	
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setMessage(R.string.outline_delete_prompt)
				.setCancelable(false)
				.setPositiveButton(R.string.yes,
						new DialogInterface.OnClickListener() {
							public void onClick(DialogInterface dialog, int id) {
								node.deleteNode(resolver);
								finish();
							}
						})
				.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int id) {
						dialog.cancel();
					}
				});
		builder.create().show();
	}
	
	private void runArchiveNode(final String editString) {	
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setMessage(R.string.outline_archive_prompt)
				.setCancelable(false)
				.setPositiveButton(R.string.yes,
						new DialogInterface.OnClickListener() {
							public void onClick(DialogInterface dialog, int id) {
								if(editString.equals("archive"))
									node.archiveNode(resolver);
								else
									node.archiveNodeToSibling(resolver);
								finish();
							}
						})
				.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int id) {
						dialog.cancel();
					}
				});
		builder.create().show();
	}

	private void runTimeClockingService() {
		Intent intent = new Intent(EditActivity.this, TimeclockService.class);
		intent.putExtra(TimeclockService.NODE_ID, node.id);
		startService(intent);
	}
	
	@Override
	public void onBackPressed() {
		doCancel();
	}
	
	private void doCancel() {
		if(hasEdits() == false) {
			setResult(RESULT_CANCELED);
			finish();
			return;
		}
		
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setMessage(R.string.node_edit_prompt)
				.setCancelable(false)
				.setPositiveButton(R.string.yes,
						new DialogInterface.OnClickListener() {
							public void onClick(DialogInterface dialog, int id) {
								setResult(RESULT_CANCELED);
								finish();
							}
						})
				.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int id) {
						dialog.cancel();
					}
				});
		builder.create().show();
	}
	
	public boolean hasEdits() {
		return false;
	}
	
	
	public void saveEdits() {
		OrgNode newNode = getEditedNode();

		if (this.actionMode.equals(ACTIONMODE_CREATE) || this.actionMode.equals(ACTIONMODE_ADDCHILD))
			createNewNode(newNode);
		else if (this.actionMode.equals(ACTIONMODE_EDIT))
			node.generateAndApplyEdits(newNode, resolver);

		announceUpdate();
	}
	
	public OrgNode getEditedNode() {
		OrgNode newParent = new OrgNode();

		boolean addTimestamp = PreferenceManager.getDefaultSharedPreferences(
				this).getBoolean("captureWithTimestamp", false);
		if(addTimestamp)
			;
		
		return this.node;
	}
	
	
	private void createNewNode(OrgNode newNode) {
		//OrgProviderUtil.createNodeWithNewheadingEditnode(newNode, newParent, newPayload, resolver);
	}

	
	private void announceUpdate() {
		Intent intent = new Intent(Synchronizer.SYNC_UPDATE);
		intent.putExtra(Synchronizer.SYNC_DONE, true);
		intent.putExtra("showToast", false);
		sendBroadcast(intent);
	}

}

<com:TActivePanel ID="Tools" Style="text-align: left; display: none; width: 492px; margin: 0 auto;">
	<com:TValidationSummary
			ID="ValidationSummary"
			CssClass="validation-error-summary"
			ValidationGroup="VolumesActionGroup"
			AutoUpdate="true"
			Display="Dynamic"
			HeaderText="<%[ There is not possible to run selected action because: ]%>" />
			<com:TActiveCustomValidator ID="LabelNameValidator" ValidationGroup="VolumesActionGroup" ControlToValidate="LabelName" ErrorMessage="<%[ Label name have to contain string value from set [0-9a-zA-Z_-]. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="labelNameValidator" />
			<com:TActiveCustomValidator ID="SlotsLabelValidator" ValidationGroup="VolumesActionGroup" ControlToValidate="SlotsLabel" ErrorMessage="<%[ Slots for label have to contain string value from set [0-9-,]. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="slotsLabelValidator" />
			<com:TActiveCustomValidator ID="DriveLabelValidator" ValidationGroup="VolumesActionGroup" ControlToValidate="DriveLabel" ErrorMessage="<%[ Drive has to contain digit value from set [0-9]. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="driveLabelValidator" />
			<com:TActiveCustomValidator ID="SlotsUpdateSlotsValidator" ValidationGroup="VolumesActionGroup" ControlToValidate="SlotsUpdateSlots" ErrorMessage="<%[ Slots for update have to contain string value from set [0-9-,]. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="slotsUpdateSlotsValidator" />
			<com:TActiveCustomValidator ID="DriveUpdateSlotsValidator" ValidationGroup="VolumesActionGroup" ControlToValidate="DriveUpdateSlots" ErrorMessage="<%[ Drive has to contain digit value from set [0-9]. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="driveUpdateSlotsValidator" />
			<com:TActiveCustomValidator ID="SlotsUpdateSlotsScanValidator" ValidationGroup="VolumesActionGroup" ControlToValidate="SlotsUpdateSlotsScan" ErrorMessage="<%[ Slots for update have to contain string value from set [0-9-,]. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="slotsUpdateSlotsScanValidator" />
			<com:TActiveCustomValidator ID="DriveUpdateSlotsScanValidator" ValidationGroup="VolumesActionGroup" ControlToValidate="DriveUpdateSlotsScan" ErrorMessage="<%[ Drive has to contain digit value from set [0-9]. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="driveUpdateSlotsScanValidator" />
	<div class="line"><com:TActiveRadioButton ID="LabelVolume" GroupName="VolumeAction" OnCallback="setLabelVolume" ActiveControl.ClientSide.OnComplete="window.scrollTo(0, document.body.scrollHeight);" /><com:TLabel ForControl="LabelVolume" Text="<%[ Label volume(s) ]%>" /></div>
	<com:TActivePanel ID="Labeling" Display="None" Style="margin-left: 20px">
		<div class="line">
			<div class="text"><com:TLabel ForControl="Barcodes" Text="<%[ Use barcodes as label: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="Barcodes" OnCallback="setBarcodes" /></div>
		</div>
		<com:TActivePanel ID="LabelNameField">
			<div class="line">
				<div class="text"><com:TLabel ForControl="LabelName" Text="<%[ Label name: ]%>" /></div>
				<div class="field"><com:TActiveTextBox ID="LabelName" CssClass="textbox" /></div>
			</div>
		</com:TActivePanel>
		<com:TActivePanel ID="BarcodeSlotsField" Display="None">
			<div class="line">
				<div class="text"><com:TLabel ForControl="SlotsLabel" Text="<%[ Slots for label: ]%>" /></div>
				<div class="field"><com:TActiveTextBox ID="SlotsLabel" CssClass="textbox" /></div>
			</div>
		</com:TActivePanel>
		<div class="line">
			<div class="text"><com:TLabel ForControl="StorageLabel" Text="<%[ Storage: ]%>" /></div>
			<div class="field"><com:TActiveDropDownList ID="StorageLabel" CssClass="textbox" /></div>
		</div>
		<div class="line">
				<div class="text"><com:TLabel ForControl="DriveLabel" Text="<%[ Drive number: ]%>" /></div>
				<div class="field"><com:TActiveTextBox ID="DriveLabel" CssClass="textbox" Text="0" /></div>
			</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="PoolLabel" Text="<%[ Pool: ]%>" /></div>
			<div class="field"><com:TActiveDropDownList ID="PoolLabel" CssClass="textbox" /></div>
		</div>
		<div class="button" style="margin-top: 10px;">
				<com:Application.Portlets.BActiveButton ID="LabelButton" Text="<%[ Label ]%>" CausesValidation="true" ValidationGroup="VolumesActionGroup" />
		</div>
	</com:TActivePanel>
	<div class="line"><com:TActiveRadioButton ID="UpdateSlots" GroupName="VolumeAction" OnCallback="setUpdateSlots" ActiveControl.ClientSide.OnComplete="window.scrollTo(0, document.body.scrollHeight);" /><com:TLabel ForControl="UpdateSlots" Text="<%[ Update slots using barcodes ]%>" /></div>
	<com:TActivePanel ID="UpdatingSlots" Display="None" Style="margin-left: 20px">
			<div class="line">
				<div class="text"><com:TLabel ForControl="StorageUpdateSlots" Text="<%[ Storage: ]%>" /></div>
				<div class="field"><com:TActiveDropDownList ID="StorageUpdateSlots" CssClass="textbox" /></div>
			</div>
			<div class="line">
				<div class="text"><com:TLabel ForControl="DriveUpdateSlots" Text="<%[ Drive number: ]%>" /></div>
				<div class="field"><com:TActiveTextBox ID="DriveUpdateSlots" CssClass="textbox" Text="0" /></div>
			</div>
			<div class="line">
				<div class="text"><com:TLabel ForControl="SlotsUpdateSlots" Text="<%[ Slots for update: ]%>" /></div>
				<div class="field"><com:TActiveTextBox ID="SlotsUpdateSlots" CssClass="textbox" /></div>
			</div>
			<div class="button" style="margin-top: 10px;">
				<com:Application.Portlets.BActiveButton ID="UpdateSlotsButton" Text="<%[ Update barcodes slots ]%>" CausesValidation="true" ValidationGroup="VolumesActionGroup" />
		</div>
	</com:TActivePanel>
	<div class="line"><com:TActiveRadioButton ID="UpdateSlotsScan" GroupName="VolumeAction" OnCallback="setUpdateSlotsScan" ActiveControl.ClientSide.OnComplete="window.scrollTo(0, document.body.scrollHeight);" /><com:TLabel ForControl="UpdateSlotsScan" Text="<%[ Update slots without barcodes ]%>" /></div>
	<com:TActivePanel ID="UpdatingSlotsScan" Display="None" Style="margin-left: 20px">
			<div class="line">
				<div class="text"><com:TLabel ForControl="StorageUpdateSlotsScan" Text="<%[ Storage: ]%>" /></div>
				<div class="field"><com:TActiveDropDownList ID="StorageUpdateSlotsScan" CssClass="textbox" /></div>
			</div>
			<div class="line">
				<div class="text"><com:TLabel ForControl="DriveUpdateSlotsScan" Text="<%[ Drive number: ]%>" /></div>
				<div class="field"><com:TActiveTextBox ID="DriveUpdateSlotsScan" CssClass="textbox" Text="0" /></div>
			</div>
			<div class="line">
				<div class="text"><com:TLabel ForControl="SlotsUpdateSlotsScan" Text="<%[ Slots for update: ]%>" /></div>
				<div class="field"><com:TActiveTextBox ID="SlotsUpdateSlotsScan" CssClass="textbox" /></div>
			</div>
			<div class="button" style="margin-top: 10px;">
				<com:Application.Portlets.BActiveButton ID="UpdateSlotsScanButton" Text="<%[ Update slots scan ]%>" CausesValidation="true" ValidationGroup="VolumesActionGroup" />
		</div>
	</com:TActivePanel>
</com:TActivePanel>
<script type="text/javascript">
	$$('#volumes_tools_launcher', '#<%=$this->LabelButton->ApplyChanges->ClientID%>', '#<%=$this->UpdateSlotsButton->ApplyChanges->ClientID%>', '#<%=$this->UpdateSlotsScanButton->ApplyChanges->ClientID%>').each(function(el){
		el.observe('click', function(){
			if(VolumesToolsValidation(el) === true) {
				$('<%=$this->getPage()->Console->ConsoleContainer->ClientID%>').hide();
				$('console_launcher').down('span').innerHTML = '<%[ show console ]%>';
				Effect.toggle('<%=$this->Tools->ClientID%>', 'slide', {'duration': 0.2, afterFinish: function(){
					window.scrollTo(0, document.body.scrollHeight);
				}});
			
				if(el.id != 'volumes_tools_launcher') {
					Effect.toggle('<%=$this->getPage()->Console->ConsoleContainer->ClientID%>', 'slide', {'duration': 0.3, afterFinish: function(){
						window.scrollTo(0, document.body.scrollHeight);
					}});
				}
			}
		}.bind(el));
	});

	function VolumesToolsValidation(el) {
		var validation = true;
		var slotsPattern = new RegExp("<%=$this->slotsPattern%>");
		var drivePattern = new RegExp("<%=$this->drivePattern%>");
		var labelNamePattern = new RegExp("<%=$this->labelVolumePattern%>");
		if(el.id == '<%=$this->LabelButton->ApplyChanges->ClientID%>') {
			if($('<%=$this->Barcodes->ClientID%>').checked === true) {
				if(slotsPattern.test($('<%=$this->SlotsLabel->ClientID%>').value) === false) {
					var validation = false;
				}
			} else {
				if(labelNamePattern.test($('<%=$this->LabelName->ClientID%>').value) === false) {
					var validation = false;
				}
			}
			if(drivePattern.test($('<%=$this->DriveLabel->ClientID%>').value) === false) {
				var validation = false;
			}
		} else if(el.id == '<%=$this->UpdateSlotsButton->ApplyChanges->ClientID%>') {
			if(drivePattern.test($('<%=$this->DriveUpdateSlots->ClientID%>').value) === false || slotsPattern.test($('<%=$this->SlotsUpdateSlots->ClientID%>').value) === false) {
				var validation = false;
			}
		} else if(el.id == '<%=$this->UpdateSlotsScanButton->ApplyChanges->ClientID%>') {
			if(drivePattern.test($('<%=$this->DriveUpdateSlotsScan->ClientID%>').value) === false || slotsPattern.test($('<%=$this->SlotsUpdateSlotsScan->ClientID%>').value) === false) {
				var validation = false;
			}
		}
		return validation;
	}
</script>
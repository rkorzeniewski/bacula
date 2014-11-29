<%@ MasterClass="Application.Portlets.ConfigurationPanel"%>
<com:TContent ID="ConfigurationWindowContent">
		<strong><%[ Storage name: ]%> <com:TActiveLabel ID="StorageName" /><com:TActiveLabel ID="StorageID" Visible="false" /></strong>
		<hr />
		<com:TValidationSummary
			ID="ValidationSummary"
			CssClass="validation-error-summary"
			ValidationGroup="AutoChangerGroup"
			AutoUpdate="true"
			Display="Dynamic"
			HeaderText="<%[ There is not possible to run selected action because: ]%>" />
		<div class="text small"><%[ Console status ]%></div>
		<div class="field-full">
			<com:TActiveTextBox ID="ShowStorage" TextMode="MultiLine" CssClass="textbox-auto" Style="height: 190px" ReadOnly="true" />
		</div>
		<com:TActivePanel ID="AutoChanger" Visible="false" Style="margin-bottom: 10px">
			<div class="line">
				<div class="text"><com:TLabel ForControl="Drive" Text="<%[ Drive number: ]%>" /></div>
				<div class="field">
					<com:TActiveTextBox ID="Drive" AutoPostBack="false" Text="0" MaxLength="3" CssClass="textbox-short" />
					<com:TActiveCustomValidator ID="DriveValidator" ValidationGroup="AutoChangerGroup" ControlToValidate="Drive" ErrorMessage="<%[ Drive number must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="driveValidator" />
				</div>
			</div>
			<div class="line">
				<div class="text"><com:TLabel ForControl="Slot" Text="<%[ Slot number: ]%>" /></div>
				<div class="field">
					<com:TActiveTextBox ID="Slot" AutoPostBack="false" Text="0" MaxLength="3" CssClass="textbox-short" />
					<com:TActiveCustomValidator ID="SlotValidator" ValidationGroup="AutoChangerGroup" ControlToValidate="Slot" ErrorMessage="<%[ Slot number must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="slotValidator" />
				</div>
			</div>
		</com:TActivePanel>
		<div class="button-center">
			<com:BActiveButton ID="Mount" OnClick="mount" ValidationGroup="AutoChangerGroup" CausesValidation="true" Text="<%[ Mount ]%>" ClientSide.OnSuccess="ConfigurationWindow.getObj('StorageWindow').progress(false);" />
			<com:BActiveButton ID="Release" OnClick="release" Text="<%[ Release ]%>" ClientSide.OnSuccess="ConfigurationWindow.getObj('StorageWindow').progress(false);" />
			<com:BActiveButton ID="Umount" OnClick="umount" ValidationGroup="AutoChangerGroup" CausesValidation="true" Text="<%[ Umount ]%>" ClientSide.OnSuccess="ConfigurationWindow.getObj('StorageWindow').progress(false);" />
			<com:BActiveButton ID="Status" OnClick="status" Text="<%[ Status ]%>" ClientSide.OnSuccess="ConfigurationWindow.getObj('StorageWindow').progress(false);" />
		</div>
</com:TContent>

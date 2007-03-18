<br/>
<div class='titlediv'>
 <h1 class='newstitle'> Ayuda para expulsar medios (part 1/2)</h1>
</div>
<div class='bodydiv'>
Se seleccionará el mejor candidato para expulsar. Se le pedirá
realizar su selección en la próxima pantalla.
  <form action="?" method='GET'>
   <table>
    <tr><td>Pool:</td>      
        <td><select name='pool' class='formulaire' multiple>
<TMPL_LOOP NAME=db_pools>
             <option selected><TMPL_VAR NAME=name></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>Tipo Medio:</td>      
        <td><select name='mediatype' class='formulaire' multiple>
<TMPL_LOOP NAME=db_mediatypes>
             <option><TMPL_VAR NAME=mediatype></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td> Ubicación : </td>
        <td><select name='location' class='formulaire'>
  <TMPL_LOOP NAME=db_locations>
      <option id='loc_<TMPL_VAR NAME=location>' value='<TMPL_VAR NAME=location>'><TMPL_VAR NAME=location></option>
  </TMPL_LOOP>
    </select>
        </td>
    </tr>
    <tr>
        <td>Número de medio <br/> a expulsar:</td> 
        <td> <input type='text' name='limit' size='3' class='formulaire' 
              value='10'> </td>
    </tr>
    <tr>
        <td><input type="image" name='action' value='compute_extern_media' title='Siguiente' src='/bweb/next.png'>
        </td><td/>
    </tr>
   </table>
   </form>
</div>
